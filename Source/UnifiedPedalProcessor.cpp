#include <JuceHeader.h>
#include "UnifiedPedalProcessor.h"
#include "Effects/DistortionEffects.h"
#include "Effects/FilterEffects.h"
#include "Effects/DelayEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/TimeRampEffect.h"
#include "Effects/ShimmerGranularEffect.h"
#include "Effects/PhaserFlangerEffect.h"
#include "Effects/MiscEffects.h"
#include <algorithm>
#include <cmath>
#include <utility>

// -------------------------------------------------------------------------
// Effect factory — creates per-chain-position instances (not per-type singletons).
namespace {

std::unique_ptr<DspEffect> createEffectForTypeImpl(DspModuleType type)
{
    switch (type)
    {
        case DspModuleType::WAVESHAPER_DISTORTION:     return std::make_unique<WaveshaperEffect>();
        case DspModuleType::SIDECHAIN_DUCKER:           return std::make_unique<SidechainDuckerEffect>();
        case DspModuleType::MATHEMATICAL_WAVEFOLDER:    return std::make_unique<WavefolderEffect>();
        case DspModuleType::BIQUAD_FILTER:              return std::make_unique<BiquadFilterEffect>();
        case DspModuleType::TIME_RAMP:                  return std::make_unique<TimeRampEffect>();
        case DspModuleType::FORMANT_VOCAL_SHIFTER:       return std::make_unique<FormantShifterEffect>();
        case DspModuleType::MICROPITCH_CHORUS:           return std::make_unique<MicroPitchChorusEffect>();
        case DspModuleType::SIMPLE_DELAY:               return std::make_unique<SimpleDelayEffect>();
        case DspModuleType::MULTI_MODE_FILTER:           return std::make_unique<MultiModeFilterEffect>();
        case DspModuleType::TAPE_STOP_REVERSE_ECHO:     return std::make_unique<TapeStopEchoEffect>();
        case DspModuleType::GRANULAR_DELAY:             return std::make_unique<GranularDelayEffect>();
        case DspModuleType::DIFFUSED_DELAY_NETWORK:     return std::make_unique<DiffusedReverbEffect>();
        case DspModuleType::PLATE_REVERB:               return std::make_unique<PlateReverbEffect>();
        case DspModuleType::PITCH_SHIFTER_GRANULAR:      return std::make_unique<GranularPitchEffect>();
        case DspModuleType::SHIMMER_GRANULAR:           return std::make_unique<ShimmerGranularEffect>();
        case DspModuleType::GLITCH_STUTTER:              return std::make_unique<GlitchStutterEffect>();
        case DspModuleType::ENVELOPE_VCA_COMPRESSOR:    return std::make_unique<VcaCompressorEffect>();
        case DspModuleType::PHASER_FLANGER:              return std::make_unique<PhaserFlangerEffect>();
        default:                                        return nullptr;
    }
}

}

// -------------------------------------------------------------------------

UnifiedPedalProcessor::UnifiedPedalProcessor()
{
    for (auto& e : m_chainEffects) e.reset();
    for (auto& t : m_chainEffectTypes) t.store(0, std::memory_order_relaxed);

    for (auto& param : m_parameterCache)
        param.store(0.5f, std::memory_order_relaxed);
}

UnifiedPedalProcessor::~UnifiedPedalProcessor()
{
    drainReleaseQueue();

    auto* current = m_currentConfig.load(std::memory_order_relaxed);
    if (current) delete current;

    auto* next = m_nextConfig.load(std::memory_order_relaxed);
    if (next) delete next;

    auto* deferred = m_deferredConfig.load(std::memory_order_relaxed);
    if (deferred) delete deferred;
}

std::unique_ptr<DspEffect> UnifiedPedalProcessor::createEffectForType(DspModuleType type)
{
    return createEffectForTypeImpl(type);
}

void UnifiedPedalProcessor::prepareToPlay(double sampleRate, int maxSamplesPerBlock, int numChannels)
{
    m_sampleRate.store(sampleRate, std::memory_order_release);
    m_maxSamplesPerBlock.store(maxSamplesPerBlock, std::memory_order_release);
    m_maxChannels.store(std::max(1, numChannels), std::memory_order_release);

    // Calculate crossfade samples from time (sample-rate independent)
    int cfSamps = std::max(1, static_cast<int>(kCrossfadeMs * sampleRate / 1000.0));
    m_crossfadeSamples.store(cfSamps, std::memory_order_release);

    // Preallocate all buffers - no allocations during processBlock
    const size_t maxSamples = static_cast<size_t>(maxSamplesPerBlock);
    const size_t maxCh = static_cast<size_t>(m_maxChannels.load(std::memory_order_relaxed));

    m_crossfadeTempBuf.resize(maxCh);
    for (auto& buf : m_crossfadeTempBuf)
        buf.assign(maxSamples, 0.0f);

    m_dryBuffer.resize(maxCh);
    for (auto& buf : m_dryBuffer)
        buf.assign(maxSamples, 0.0f);

    m_crossfadeOldOut.resize(maxCh);
    for (auto& buf : m_crossfadeOldOut)
        buf.assign(maxSamples, 0.0f);

    // chain effects are prepared per-config in loadPedalConfiguration

    reset();
}

void UnifiedPedalProcessor::reset()
{
    for (auto& effect : m_chainEffects)
        if (effect)
            effect->reset();

    m_crossfadeCounter = 0;
}

void UnifiedPedalProcessor::prebuildEffects(const PedalAssetPayload* config, bool& deferred)
{
    deferred = false;
    if (!config) return;

    // If any crossfade is active (m_nextConfig non-null), the audio thread
    // may be reading m_pendingEffects. Never write to them during crossfade.
    if (m_nextConfig.load(std::memory_order_acquire) != nullptr)
    {
        deferred = true;
        return;
    }

    bool touched[PedalSlotCount]{};
    double sr = m_sampleRate.load(std::memory_order_relaxed);
    int maxCh = m_maxChannels.load(std::memory_order_relaxed);

    for (size_t i = 0; i < m_chainEffects.size(); ++i)
    {
        DspModuleType neededType = (i < config->activeRoutingChain.size())
            ? config->activeRoutingChain[i]
            : DspModuleType::BYPASS;
        if (static_cast<DspModuleType>(m_chainEffectTypes[i].load(std::memory_order_acquire)) != neededType
            || !m_chainEffects[i])
        {
            touched[i] = true;
            if (neededType != DspModuleType::BYPASS)
            {
                m_pendingEffects[i] = createEffectForType(neededType);
                if (m_pendingEffects[i])
                    m_pendingEffects[i]->prepare(sr, maxCh);
            }
            else
            {
                m_pendingEffects[i].reset();
            }
        }
    }

    for (size_t i = 0; i < PedalSlotCount; ++i)
        if (!touched[i])
            m_pendingEffects[i].reset();
}

void UnifiedPedalProcessor::loadPedalConfiguration(const PedalAssetPayload* config)
{
    if (!config) return;

    const auto* current = m_currentConfig.load(std::memory_order_acquire);
    // Discard any stale deferred config before processing a new one
    {
        auto* stale = m_deferredConfig.exchange(nullptr, std::memory_order_acq_rel);
        if (stale)
            pushToReleaseQueue(stale);
    }

    if (current && !current->activeRoutingChain.empty())
    {
        bool prebuildDeferred = false;
        prebuildEffects(config, prebuildDeferred);
        if (prebuildDeferred)
        {
            // Crossfade in progress — can't touch m_pendingEffects.
            // Stash config and retry after the crossfade completes.
            m_deferredConfig.store(config, std::memory_order_release);
            return;
        }
        // No crossfade active — safe to publish m_nextConfig.
        // m_pendingEffects now contains the effects for this config.
        m_nextConfig.store(config, std::memory_order_release);
        m_pendingCrossfadeReset.store(true, std::memory_order_release);
    }
    else
    {
        // First load: build effects, swap into chain, then publish m_currentConfig.
        // No active config yet — audio thread returns early (null current check).
        bool unused = false;
        prebuildEffects(config, unused);
        for (size_t i = 0; i < m_chainEffects.size(); ++i)
            if (m_pendingEffects[i])
                std::swap(m_chainEffects[i], m_pendingEffects[i]);

        for (size_t i = 0; i < config->activeRoutingChain.size(); ++i)
            m_chainEffectTypes[i].store(static_cast<uint8_t>(config->activeRoutingChain[i]),
                                        std::memory_order_release);

        reset();

        const auto* oldCurrent = m_currentConfig.exchange(config, std::memory_order_acq_rel);
        if (oldCurrent)
            pushToReleaseQueue(oldCurrent);
    }
}

void UnifiedPedalProcessor::tryApplyDeferredConfig()
{
    auto* deferred = m_deferredConfig.exchange(nullptr, std::memory_order_acq_rel);
    if (deferred)
    {
        // Only try if no crossfade is in progress
        if (m_nextConfig.load(std::memory_order_acquire) == nullptr)
            loadPedalConfiguration(deferred);
        else
            m_deferredConfig.store(deferred, std::memory_order_release); // still blocked
    }
}

float UnifiedPedalProcessor::readParam(uint16_t token, float fallback,
                                       const PedalAssetPayload& config, uint8_t nodeIndex) const
{
    if (static_cast<size_t>(nodeIndex) < config.routingSlotOrder.size())
    {
        int physicalSlot = config.routingSlotOrder[static_cast<size_t>(nodeIndex)];
        size_t cacheIdx = static_cast<size_t>(physicalSlot * 4 + token);
        if (cacheIdx < 24)
        {
            uint32_t validMask = m_paramCacheValidMask.load(std::memory_order_acquire);
            if ((validMask & (1u << cacheIdx)) != 0)
                return m_parameterCache[cacheIdx].load(std::memory_order_relaxed);
        }
    }

    for (auto& p : config.parameters)
        if (p.parameterToken == token && p.targetDspNodeRegister == nodeIndex)
            return p.currentValue;
    return fallback;
}

std::vector<ParameterDescriptor> UnifiedPedalProcessor::getCurrentParams() const
{
    const auto* current = m_currentConfig.load(std::memory_order_acquire);
    if (current)
        return current->parameters;
    return {};
}

const PedalAssetPayload* UnifiedPedalProcessor::getCurrentConfig() const
{
    return m_currentConfig.load(std::memory_order_acquire);
}

void UnifiedPedalProcessor::updateParameter(int physicalSlot, int knobIdx, float newValue)
{
    if (!std::isfinite(newValue))
        newValue = 0.0f;

    // Lock-free atomic write for audio thread consumption
    if (physicalSlot >= 0 && physicalSlot < PedalSlotCount && knobIdx >= 0 && knobIdx < 4)
    {
        const size_t idx = static_cast<size_t>(physicalSlot * 4 + knobIdx);
        m_parameterCache[idx].store(newValue, std::memory_order_release);
        uint32_t mask = m_paramCacheValidMask.load(std::memory_order_relaxed);
        mask |= (1u << idx);
        m_paramCacheValidMask.store(mask, std::memory_order_release);
        m_paramRevision.fetch_add(1, std::memory_order_acq_rel);
    }
}

void UnifiedPedalProcessor::invalidateParamCacheForSlot(int physicalSlot)
{
    if (physicalSlot < 0 || physicalSlot >= PedalSlotCount) return;
    uint32_t mask = m_paramCacheValidMask.load(std::memory_order_relaxed);
    for (int k = 0; k < 4; ++k)
        mask &= ~(1u << static_cast<size_t>(physicalSlot * 4 + k));
    m_paramCacheValidMask.store(mask, std::memory_order_release);
}

void UnifiedPedalProcessor::scheduleReset()
{
    m_pendingReset.store(true, std::memory_order_release);
}

bool UnifiedPedalProcessor::isParamOverridden(int physicalSlot, int knobIdx) const
{
    if (physicalSlot < 0 || physicalSlot >= PedalSlotCount || knobIdx < 0 || knobIdx >= 4)
        return false;
    size_t idx = static_cast<size_t>(physicalSlot * 4 + knobIdx);
    uint32_t mask = m_paramCacheValidMask.load(std::memory_order_acquire);
    return (mask & (1u << idx)) != 0;
}

bool UnifiedPedalProcessor::hasPendingReleases() const
{
    if (m_audioReleasePtr.load(std::memory_order_acquire) != nullptr)
        return true;
    int rIdx = m_releaseReadIndex.load(std::memory_order_acquire);
    int wIdx = m_releaseWriteIndex.load(std::memory_order_acquire);
    return rIdx != wIdx;
}

UnifiedPedalProcessor::ParameterSnapshot UnifiedPedalProcessor::getSnapshot() const
{
    ParameterSnapshot snap;
    snap.revision = m_paramRevision.load(std::memory_order_acquire);

    for (size_t i = 0; i < snap.values.size(); ++i)
        snap.values[i] = m_parameterCache[i].load(std::memory_order_relaxed);

    return snap;
}

void UnifiedPedalProcessor::processChainBlock(float** b, int c, int s, const PedalAssetPayload& config,
                                              std::array<std::unique_ptr<DspEffect>, PedalSlotCount>& effects)
{
    int chCount = std::min(c, m_maxChannels.load(std::memory_order_relaxed));

    for (int idx = 0; idx < static_cast<int>(config.activeRoutingChain.size()); ++idx)
    {
        auto* effectPtr = effects[static_cast<size_t>(idx)].get();
        if (!effectPtr)
            effectPtr = m_chainEffects[static_cast<size_t>(idx)].get();

        if (effectPtr)
        {
            uint32_t mask = m_paramCacheValidMask.load(std::memory_order_acquire);
            int physSlot = config.routingSlotOrder[static_cast<size_t>(idx)];
            size_t baseIdx = static_cast<size_t>(physSlot) * 4;
            auto readCached = [&](uint16_t token, float fb) -> float {
                size_t ci = baseIdx + token;
                if ((mask & (1u << ci)) != 0)
                    return m_parameterCache[ci].load(std::memory_order_relaxed);
                for (auto& p : config.parameters)
                    if (p.parameterToken == token && p.targetDspNodeRegister == static_cast<uint8_t>(idx))
                        return p.currentValue;
                return fb;
            };

            float wet = readCached(static_cast<uint16_t>(ParamToken::Wet), 0.5f);
            float dryLevel = readCached(static_cast<uint16_t>(ParamToken::Dry), 0.5f);
            float volumeParam = readCached(static_cast<uint16_t>(ParamToken::Volume), 0.5f);
            float effectParam = readCached(static_cast<uint16_t>(ParamToken::Effect), 0.5f);

            effectPtr->setVolumeParam(volumeParam);

            bool needDry = (dryLevel != 0.0f);
            if (needDry)
                for (int ch = 0; ch < chCount; ++ch)
                    for (int smp = 0; smp < s; ++smp)
                        m_dryBuffer[static_cast<size_t>(ch)][static_cast<size_t>(smp)] = b[ch][smp];

            effectPtr->processBlock(b, c, s, effectParam);

            for (int ch = 0; ch < chCount; ++ch)
                for (int smp = 0; smp < s; ++smp)
                    b[ch][smp] = (needDry ? m_dryBuffer[static_cast<size_t>(ch)][static_cast<size_t>(smp)] * dryLevel : 0.0f) + b[ch][smp] * wet;
        }
    }
}

void UnifiedPedalProcessor::processAudioBlock(float** buffer, int numChannels, int numSamples)
{
    int maxSamples = m_maxSamplesPerBlock.load(std::memory_order_relaxed);
    if (numSamples > maxSamples) numSamples = maxSamples;

    if (m_pendingReset.exchange(false, std::memory_order_acq_rel))
        reset();

    if (m_pendingCrossfadeReset.exchange(false, std::memory_order_acq_rel))
        m_crossfadeCounter = 0;

    const auto* current = m_currentConfig.load(std::memory_order_acquire);
    const auto* next = m_nextConfig.load(std::memory_order_acquire);

    if (!current)
        return;

    if (!next)
    {
        bool silent = true;
        int checkCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));
        for (int c = 0; c < checkCh; ++c)
            for (int s = 0; s < std::min(4, numSamples); ++s)
                if (std::abs(buffer[c][s]) > 1e-6f) { silent = false; break; }

        if (silent)
        {
            ++m_silentBlockCount;
            if (m_silentBlockCount >= 3)
                return;
        }
        else
        {
            m_silentBlockCount = 0;
        }
    }

    int crossfadeSamps = m_crossfadeSamples.load(std::memory_order_relaxed);
    int copyCh = std::min(numChannels, m_maxChannels.load(std::memory_order_relaxed));

    if (next && m_crossfadeCounter.load(std::memory_order_relaxed) < crossfadeSamps)
    {
        // Save entire input block before processing
        for (int ch = 0; ch < copyCh; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
                m_crossfadeTempBuf[static_cast<size_t>(ch)][static_cast<size_t>(smp)] = buffer[ch][smp];

        // Process current config on original buffer
        processChainBlock(buffer, numChannels, numSamples, *current, m_chainEffects);

        // Save current output
        for (int ch = 0; ch < copyCh; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
                m_crossfadeOldOut[static_cast<size_t>(ch)][static_cast<size_t>(smp)] = buffer[ch][smp];

        // Restore original input
        for (int ch = 0; ch < copyCh; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
                buffer[ch][smp] = m_crossfadeTempBuf[static_cast<size_t>(ch)][static_cast<size_t>(smp)];

        // Process next config on restored input
        processChainBlock(buffer, numChannels, numSamples, *next, m_pendingEffects);

        // Crossfade mix per sample + limiter
        {
            int cc = m_crossfadeCounter.load(std::memory_order_relaxed);
            for (int smp = 0; smp < numSamples; ++smp)
            {
                float g = std::min(1.0f, static_cast<float>(cc) / static_cast<float>(crossfadeSamps));
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    float x = m_crossfadeOldOut[static_cast<size_t>(ch)][static_cast<size_t>(smp)] * (1.0f - g)
                            + buffer[ch][smp] * g;
                    float absX = std::abs(x);
                    if (absX > 0.85f)
                    {
                        float over = absX - 0.85f;
                        float compressed = 0.85f + over / 3.0f;
                        if (compressed > 1.0f) compressed = 1.0f;
                        x = (x >= 0.0f) ? compressed : -compressed;
                    }
                    buffer[ch][smp] = std::isfinite(x) ? juce::jlimit(-1.0f, 1.0f, x) : 0.0f;
                }
                ++cc;
            }
            m_crossfadeCounter.store(cc, std::memory_order_relaxed);
        }
    }
    else if (next && m_crossfadeCounter.load(std::memory_order_relaxed) >= crossfadeSamps)
    {
        processChainBlock(buffer, numChannels, numSamples, *next, m_pendingEffects);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
            {
                float x = buffer[ch][smp];
                float absX = std::abs(x);
                if (absX > 0.85f)
                {
                    float over = absX - 0.85f;
                    float compressed = 0.85f + over / 3.0f;
                    if (compressed > 1.0f) compressed = 1.0f;
                    x = (x >= 0.0f) ? compressed : -compressed;
                }
                buffer[ch][smp] = std::isfinite(x) ? juce::jlimit(-1.0f, 1.0f, x) : 0.0f;
            }
    }
    else
    {
        processChainBlock(buffer, numChannels, numSamples, *current, m_chainEffects);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
            {
                float x = buffer[ch][smp];
                float absX = std::abs(x);
                if (absX > 0.85f)
                {
                    float over = absX - 0.85f;
                    float compressed = 0.85f + over / 3.0f;
                    if (compressed > 1.0f) compressed = 1.0f;
                    x = (x >= 0.0f) ? compressed : -compressed;
                }
                buffer[ch][smp] = std::isfinite(x) ? juce::jlimit(-1.0f, 1.0f, x) : 0.0f;
            }
    }

    if (next && m_crossfadeCounter.load(std::memory_order_relaxed) >= crossfadeSamps)
    {
        if (m_nextConfig.load(std::memory_order_acquire) != next)
        {
            m_crossfadeCounter.store(0, std::memory_order_relaxed);
            return;
        }

        const auto* oldCurrent = m_currentConfig.exchange(next, std::memory_order_acq_rel);
        if (oldCurrent)
            m_audioReleasePtr.store(oldCurrent, std::memory_order_release);
        m_nextConfig.store(nullptr, std::memory_order_release);

        for (size_t i = 0; i < PedalSlotCount; ++i)
            std::swap(m_chainEffects[i], m_pendingEffects[i]);

        for (size_t i = 0; i < PedalSlotCount; ++i)
        {
            uint8_t typeVal = (i < next->activeRoutingChain.size())
                ? static_cast<uint8_t>(next->activeRoutingChain[i])
                : 0;
            m_chainEffectTypes[i].store(typeVal, std::memory_order_release);
        }

        m_crossfadeCounter.store(0, std::memory_order_relaxed);
        reset();
    }
}

void UnifiedPedalProcessor::pushToReleaseQueue(const PedalAssetPayload* ptr)
{
    int wIdx = m_releaseWriteIndex.load(std::memory_order_relaxed);
    int nextW = (wIdx + 1) % kReleaseQueueCapacity;
    if (nextW == m_releaseReadIndex.load(std::memory_order_acquire))
        return;
    m_releaseQueue[static_cast<size_t>(wIdx)] = ptr;
    m_releaseWriteIndex.store(nextW, std::memory_order_release);
}

void UnifiedPedalProcessor::drainReleaseQueue()
{
    // Drain audio thread release first (single atomic slot, lock-free)
    auto* audioPtr = m_audioReleasePtr.exchange(nullptr, std::memory_order_acq_rel);
    delete audioPtr;

    // Drain UI-thread ring buffer (SPSC, producer is UI thread only)
    int rIdx = m_releaseReadIndex.load(std::memory_order_relaxed);
    int wIdx = m_releaseWriteIndex.load(std::memory_order_acquire);
    while (rIdx != wIdx)
    {
        delete m_releaseQueue[static_cast<size_t>(rIdx)];
        rIdx = (rIdx + 1) % kReleaseQueueCapacity;
    }
    m_releaseReadIndex.store(rIdx, std::memory_order_release);
}
