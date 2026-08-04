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
    m_sampleRate.store(sampleRate, std::memory_order_release);
    m_maxSamplesPerBlock.store(maxSamplesPerBlock, std::memory_order_release);
    m_maxChannels.store(std::max(1, numChannels), std::memory_order_release);

    const size_t maxSamples = static_cast<size_t>(maxSamplesPerBlock);
    const size_t maxCh = static_cast<size_t>(m_maxChannels.load(std::memory_order_relaxed));

    constexpr float kAutoSmoothHz = 12.0f;
    m_autoSmoothAlpha = 1.0f - std::exp(-2.0f * 3.14159265f * kAutoSmoothHz * static_cast<float>(maxSamplesPerBlock) / static_cast<float>(sampleRate));

    m_crossfade.prepare(sampleRate, maxSamplesPerBlock, maxCh);

    m_dryBuffer.resize(maxCh);
    for (auto& buf : m_dryBuffer)
        buf.assign(maxSamples, 0.0f);
}

void UnifiedPedalProcessor::reset(std::array<std::unique_ptr<DspEffect>, PedalSlotCount>& effects)
{
    for (auto& effect : effects)
        if (effect)
            effect->reset();

    m_crossfade.reset();
}

void UnifiedPedalProcessor::setKnobParameter(int physicalSlot, int knobIdx, float dragStartValue, float newValue)
{
    if (physicalSlot >= 0 && physicalSlot < PedalSlotCount && knobIdx >= 0 && knobIdx < 4)
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

void UnifiedPedalProcessor::processChainBlock(float** b, int c, int s, const PedalAssetPayload& config,
                                              std::array<std::unique_ptr<DspEffect>, PedalSlotCount>& effects,
                                              std::array<std::array<const float*, 4>, PedalSlotCount>& paramPtrs)
{
    juce::ScopedNoDenormals noDenorm;
    int chCount = std::min(c, m_maxChannels.load(std::memory_order_relaxed));

    for (int idx = 0; idx < static_cast<int>(config.activeRoutingChain.size()); ++idx)
    {
        auto* effectPtr = effects[static_cast<size_t>(idx)].get();
        if (!effectPtr)
            continue;

        uint32_t mask = m_paramCache.readValidMask();
        if (static_cast<size_t>(idx) >= config.routingSlotOrder.size())
            continue;
        int physSlot = config.routingSlotOrder[static_cast<size_t>(idx)];
        size_t baseIdx = static_cast<size_t>(physSlot) * 4;

        float params[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        auto& row = paramPtrs[static_cast<size_t>(idx)];
        for (int k = 0; k < 4; ++k)
        {
            size_t ci = baseIdx + static_cast<size_t>(k);
            if ((mask & (1u << ci)) != 0)
                params[k] = m_paramCache.readRaw(static_cast<int>(ci));
            else if (row[static_cast<size_t>(k)] != nullptr)
                params[k] = *row[static_cast<size_t>(k)];
        }

        float rawAuto = m_currentAutomationValue.load(std::memory_order_relaxed);
        m_smoothedAutoValue += (rawAuto - m_smoothedAutoValue) * m_autoSmoothAlpha;
        float autoVal = m_smoothedAutoValue;
        for (int k = 0; k < 4; ++k)
            if (m_pedalState.knobLinked(physSlot, k))
                params[k] = params[k] * (1.0f - m_pedalState.knobLinkStrength(physSlot, k)) + autoVal * m_pedalState.knobLinkStrength(physSlot, k);

        int mi = effectPtr->mixKnobIndex();
        bool hasMix = (mi >= 0 && mi < 4);
        if (hasMix)
        {
            float prevMix = m_prevMix[static_cast<size_t>(physSlot)];
            float currMix = params[mi];
            if (prevMix < 1.0f || currMix < 1.0f)
                for (int ch = 0; ch < chCount; ++ch)
                    for (int smp = 0; smp < s; ++smp)
                        m_dryBuffer[static_cast<size_t>(ch)][static_cast<size_t>(smp)] = b[ch][smp];
        }

        effectPtr->processBlock(b, chCount, s, params);

        float prevMix = hasMix ? m_prevMix[static_cast<size_t>(physSlot)] : 1.0f;
        float currMix = hasMix ? params[mi] : 1.0f;
        if (hasMix)
            m_prevMix[static_cast<size_t>(physSlot)] = currMix;

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
                x = juce::jlimit(-1.0f, 1.0f, x);
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

    if (m_pendingReset.exchange(false, std::memory_order_acq_rel))
        reset(cfg.chainEffects);

    if (m_crossfade.consumeResetRequest())
        m_crossfade.reset();

    const auto* current = cfg.currentConfig.load(std::memory_order_acquire);
    const auto* next = cfg.nextConfig.load(std::memory_order_acquire);

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

    if (!next)
    {
        bool silent = true;
        int checkCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));
        for (int c = 0; c < checkCh; ++c)
            for (int s = 0; s < std::min(16, numSamples); ++s)
                if (std::abs(buffer[c][s]) > 1e-6f) { silent = false; break; }

        if (silent)
        {
            ++m_silentBlockCount;
            if (m_silentBlockCount >= 3)
                return;
        }
        else
        {
            if (m_silentBlockCount >= 3)
                reset(cfg.chainEffects);
            m_silentBlockCount = 0;
        }
    }

    int copyCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));

    if (next && m_crossfade.isActive())
    {
        m_crossfade.copyToTemp(buffer, copyCh, numSamples);
        processChainBlock(buffer, numChannels, numSamples, *current, cfg.chainEffects, cfg.paramPtrs);
        m_crossfade.captureOldOut(buffer, copyCh, numSamples);
        m_crossfade.restoreInput(buffer, copyCh, numSamples);
        processChainBlock(buffer, numChannels, numSamples, *next, cfg.pendingEffects, cfg.paramPtrs);
        m_crossfade.fadeOutputs(buffer, copyCh, numSamples);
    }
    else if (next && m_crossfade.isComplete())
    {
        processChainBlock(buffer, numChannels, numSamples, *next, cfg.pendingEffects, cfg.paramPtrs);
    }
    else
    {
        processChainBlock(buffer, numChannels, numSamples, *current, cfg.chainEffects, cfg.paramPtrs);
    }

    float outGain = m_pedalState.getOutputGain();
    int gainCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));
    for (int ch = 0; ch < gainCh; ++ch)
        for (int smp = 0; smp < numSamples; ++smp)
        {
            float x = buffer[ch][smp] * outGain;
            buffer[ch][smp] = std::isfinite(x) ? std::tanh(x) : 0.0f;
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

        for (size_t i = 0; i < PedalSlotCount; ++i)
            std::swap(cfg.chainEffects[i], cfg.pendingEffects[i]);

        m_crossfade.reset();
        reset(cfg.chainEffects);
    }
}
