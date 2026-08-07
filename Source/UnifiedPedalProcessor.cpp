#include <JuceHeader.h>
#include "UnifiedPedalProcessor.h"
#include "State/ConfigManager.h"
#include <algorithm>
#include <cmath>
#include <utility>

UnifiedPedalProcessor::UnifiedPedalProcessor()
{
}

UnifiedPedalProcessor::~UnifiedPedalProcessor()
{
}

void UnifiedPedalProcessor::prepareToPlay(double sampleRate, int maxSamplesPerBlock, int numChannels)
{
    if (sampleRate < 1.0)
        sampleRate = 44100.0;
    if (maxSamplesPerBlock < 1)
        maxSamplesPerBlock = 1;

    m_sampleRate.store(sampleRate, std::memory_order_release);
    m_maxSamplesPerBlock.store(maxSamplesPerBlock, std::memory_order_release);
    m_maxChannels.store(std::max(1, numChannels), std::memory_order_release);

    const size_t maxSamples = static_cast<size_t>(maxSamplesPerBlock);
    const size_t maxCh = static_cast<size_t>(m_maxChannels.load(std::memory_order_relaxed));

    constexpr float kAutoSmoothHz = 12.0f;
    m_autoSmoothAlpha = 1.0f - std::exp(-2.0f * 3.14159265f * kAutoSmoothHz * static_cast<float>(maxSamplesPerBlock) / static_cast<float>(sampleRate));

    constexpr float kParamSmoothHz = 40.0f;
    m_paramSmoothAlpha = 1.0f - std::exp(-2.0f * 3.14159265f * kParamSmoothHz * static_cast<float>(maxSamplesPerBlock) / static_cast<float>(sampleRate));

    m_crossfade.prepare(sampleRate, maxSamplesPerBlock, maxCh);

    m_dryBuffer.resize(maxCh);
    for (auto& buf : m_dryBuffer)
        buf.assign(maxSamples, 0.0f);
}

namespace
{
float softClip(float x)
{
    constexpr float knee = 0.85f;
    float ax = std::abs(x);
    if (ax <= knee)
        return x;
    float y = knee + (1.0f - knee) * std::tanh((ax - knee) / (1.0f - knee));
    return (x >= 0.0f) ? y : -y;
}
}

void UnifiedPedalProcessor::reset(const PedalAssetPayload& config)
{
    for (auto& effect : config.effects)
        if (effect)
            effect->reset();

    m_crossfade.reset();
}

void UnifiedPedalProcessor::setKnobParameter(int physicalSlot, int knobIdx, float dragStartValue, float newValue)
{
    if (physicalSlot >= 0 && physicalSlot < PedalSlotCount && knobIdx >= 0 && knobIdx < KnobsPerPedal)
    {
        if (m_pedalState.isKnobLinked(physicalSlot, knobIdx))
            m_pedalState.setKnobLink(physicalSlot, knobIdx, false, 0.0f);
        m_paramCache.applyOffset(physicalSlot, knobIdx, dragStartValue, newValue);
    }
}

void UnifiedPedalProcessor::scheduleReset()
{
    m_pendingReset.store(true, std::memory_order_release);
}

void UnifiedPedalProcessor::processChainBlock(float** b, int c, int s, const PedalAssetPayload& config)
{
    juce::ScopedNoDenormals noDenorm;
    int chCount = std::min(c, m_maxChannels.load(std::memory_order_relaxed));

    for (int idx = 0; idx < static_cast<int>(config.activeRoutingChain.size()); ++idx)
    {
        auto* effectPtr = config.effects[static_cast<size_t>(idx)].get();
        if (!effectPtr)
            continue;

        uint32_t mask = m_paramCache.readValidMask();
        if (static_cast<size_t>(idx) >= config.routingSlotOrder.size())
            continue;
        int physSlot = config.routingSlotOrder[static_cast<size_t>(idx)];
        size_t baseIdx = static_cast<size_t>(physSlot) * KnobsPerPedal;

        float params[KnobsPerPedal] = {0.5f, 0.5f, 0.5f, 0.5f};
        const auto& row = config.paramPtrs[static_cast<size_t>(idx)];
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            size_t ci = baseIdx + static_cast<size_t>(k);
            if ((mask & (1u << ci)) != 0)
                params[k] = m_paramCache.readRaw(static_cast<int>(ci));
            else if (row[static_cast<size_t>(k)] != nullptr)
                params[k] = *row[static_cast<size_t>(k)];
        }

        int mi = effectPtr->mixKnobIndex();
        bool hasMix = (mi >= 0 && mi < KnobsPerPedal);

        float rawAuto = m_currentAutomationValue.load(std::memory_order_relaxed);
        m_smoothedAutoValue += (rawAuto - m_smoothedAutoValue) * m_autoSmoothAlpha;
        float autoVal = m_smoothedAutoValue;
        for (int k = 0; k < KnobsPerPedal; ++k)
            if (k != mi && m_pedalState.knobLinked(physSlot, k))
                params[k] = params[k] * (1.0f - m_pedalState.knobLinkStrength(physSlot, k)) + autoVal * m_pedalState.knobLinkStrength(physSlot, k);

        auto& smoothRow = m_smoothedParams[static_cast<size_t>(physSlot)];
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            if (k == mi)
                continue;
            smoothRow[static_cast<size_t>(k)] += (params[k] - smoothRow[static_cast<size_t>(k)]) * m_paramSmoothAlpha;
            params[k] = smoothRow[static_cast<size_t>(k)];
        }

        const float driftAmount = m_driftAmounts[static_cast<size_t>(physSlot)].load(std::memory_order_acquire);
        if (driftAmount > 0.0001f)
        {
            auto& dm = m_drift[static_cast<size_t>(physSlot)];
            const float srF = static_cast<float>(m_sampleRate.load(std::memory_order_relaxed));
            const float blockSec = (srF > 0.0f) ? static_cast<float>(s) / srF : 0.0f;
            const float driftRate = 0.3f + driftAmount * 2.7f;
            const float unstableRate = 6.0f + driftAmount * 9.0f;

            auto updateMod = [](float& phase, float& value, float& target, uint32_t& rng,
                                float rateHz, float blockSec, float maxMod, float tau)
            {
                phase += rateHz * blockSec;
                if (phase >= 1.0f)
                {
                    phase -= 1.0f;
                    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                    const float r = static_cast<float>(rng & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
                    target = r * maxMod;
                }
                const float alpha = (blockSec > 0.0f) ? 1.0f - std::exp(-blockSec / tau) : 0.0f;
                value += (target - value) * alpha;
            };

            const float tau = 0.5f + (1.0f - driftAmount) * 1.5f;
            updateMod(dm.phase, dm.value, dm.target, dm.rngState, driftRate, blockSec, driftAmount * 0.02f, tau);
            updateMod(dm.phaseUnstable, dm.valueUnstable, dm.targetUnstable, dm.rngState, unstableRate, blockSec, driftAmount * 0.008f, 0.08f);

            const float mod = dm.value + dm.valueUnstable;
            for (int k = 0; k < KnobsPerPedal; ++k)
            {
                if (k == mi)
                    continue;
                params[k] += mod;
                if (params[k] < 0.0f) params[k] = 0.0f;
                else if (params[k] > 1.0f) params[k] = 1.0f;
            }
        }

        if (hasMix)
        {
            float prevMix = m_prevMix[static_cast<size_t>(physSlot)];
            float currMix = params[mi];
            if (prevMix < 1.0f || currMix < 1.0f)
                for (int ch = 0; ch < chCount; ++ch)
                    for (int smp = 0; smp < s; ++smp)
                        m_dryBuffer[static_cast<size_t>(ch)][static_cast<size_t>(smp)] = b[ch][smp];
        }

        const bool fullyDry = (hasMix && m_prevMix[static_cast<size_t>(physSlot)] == 0.0f && params[mi] == 0.0f);
        if (!(fullyDry && !effectPtr->hasActiveTail()))
            effectPtr->processBlock(b, chCount, s, params);

        float prevMix = hasMix ? m_prevMix[static_cast<size_t>(physSlot)] : 1.0f;
        float currMix = hasMix ? params[mi] : 1.0f;
        if (hasMix)
        {
            if (prevMix < 0.0f)
                prevMix = currMix;
            m_prevMix[static_cast<size_t>(physSlot)] = currMix;
        }

        float pedalGain = m_pedalState.gainRef(physSlot).load(std::memory_order_relaxed);
        float peak = 0.0f;
        for (int ch = 0; ch < chCount; ++ch)
            for (int smp = 0; smp < s; ++smp)
            {
                float x = b[ch][smp];
                if (hasMix)
                {
                    float t = static_cast<float>(smp) / static_cast<float>(s);
                    float mix = prevMix + (currMix - prevMix) * t;
                    x = m_dryBuffer[static_cast<size_t>(ch)][static_cast<size_t>(smp)] * (1.0f - mix) + x * mix;
                }
                if (!std::isfinite(x)) x = 0.0f;
                if (pedalGain != 1.0f) x *= pedalGain;
                x = softClip(x);
                b[ch][smp] = x;
                peak = std::max(peak, std::abs(x));
            }
        m_pedalState.peakRef(physSlot).store(peak, std::memory_order_relaxed);
    }
}

void UnifiedPedalProcessor::processAudioBlock(float** buffer, int numChannels, int numSamples,
                                              ConfigAudioView& cfg)
{
    juce::ScopedNoDenormals noDenormals;
    int maxSamples = m_maxSamplesPerBlock.load(std::memory_order_relaxed);
    if (numSamples > maxSamples) numSamples = maxSamples;

    if (m_crossfade.consumeResetRequest())
        m_crossfade.reset();

    const auto* current = cfg.currentConfig.load(std::memory_order_acquire);
    const auto* next = cfg.nextConfig.load(std::memory_order_acquire);

    if (next != nullptr && next == current)
        cfg.nextConfig.store(nullptr, std::memory_order_release);

    if (m_pendingReset.exchange(false, std::memory_order_acq_rel) && current)
        reset(*current);

    if (!current)
        return;

    float inGain = m_pedalState.getInputGain();
    if (inGain != 1.0f)
    {
        int gainCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));
        for (int ch = 0; ch < gainCh; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
                buffer[ch][smp] *= inGain;
    }

    int copyCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));

    if (next && m_crossfade.isActive())
    {
        m_crossfade.copyToTemp(buffer, copyCh, numSamples);
        processChainBlock(buffer, numChannels, numSamples, *current);
        m_crossfade.captureOldOut(buffer, copyCh, numSamples);
        m_crossfade.restoreInput(buffer, copyCh, numSamples);
        processChainBlock(buffer, numChannels, numSamples, *next);
        m_crossfade.fadeOutputs(buffer, copyCh, numSamples);
    }
    else if (next && m_crossfade.isComplete())
    {
        processChainBlock(buffer, numChannels, numSamples, *next);
    }
    else
    {
        processChainBlock(buffer, numChannels, numSamples, *current);
    }

    float outGain = m_pedalState.getOutputGain();
    int gainCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));
    if (outGain == 1.0f)
    {
        for (int ch = 0; ch < gainCh; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
            {
                float x = buffer[ch][smp];
                buffer[ch][smp] = std::isfinite(x) ? softClip(x) : 0.0f;
            }
    }
    else
    {
        for (int ch = 0; ch < gainCh; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
            {
                float x = buffer[ch][smp] * outGain;
                buffer[ch][smp] = std::isfinite(x) ? softClip(x) : 0.0f;
            }
    }

    if (next && m_crossfade.isComplete() && m_crossfade.counter() >= m_crossfade.samples())
    {
        const auto* expected = next;
        if (!cfg.nextConfig.compare_exchange_strong(expected, nullptr,
                std::memory_order_acq_rel, std::memory_order_acquire))
        {
            m_crossfade.reset();
            return;
        }

        const auto* oldCurrent = cfg.currentConfig.exchange(next, std::memory_order_acq_rel);
        if (oldCurrent)
            cfg.releaseQueue.pushSingle(oldCurrent);

        m_crossfade.reset();
        reset(*next);
    }
}
