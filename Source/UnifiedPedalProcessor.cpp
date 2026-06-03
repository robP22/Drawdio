#include "UnifiedPedalProcessor.h"
#include "Effects/DistortionEffects.h"
#include "Effects/FilterEffects.h"
#include "Effects/DelayEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/MiscEffects.h"
#include <algorithm>
#include <mutex>
#include <utility>

// -------------------------------------------------------------------------
// Effect registration helper.
namespace {

template<typename T>
void reg(std::array<std::unique_ptr<DspEffect>, 19>& effects, DspModuleType type)
{
    effects[static_cast<int>(type)] = std::make_unique<T>();
}

void registerAllEffects(std::array<std::unique_ptr<DspEffect>, 19>& effects)
{
    reg<WaveshaperEffect>(effects, DspModuleType::WAVESHAPER_DISTORTION);
    reg<SoftDistortionEffect>(effects, DspModuleType::SOFT_DISTORTION);
    reg<WavefolderEffect>(effects, DspModuleType::MATHEMATICAL_WAVEFOLDER);
    reg<BiquadFilterEffect>(effects, DspModuleType::BIQUAD_FILTER);
    reg<AllpassCascadeEffect>(effects, DspModuleType::ALLPASS_FILTER_CASCADE);
    reg<FormantShifterEffect>(effects, DspModuleType::FORMANT_VOCAL_SHIFTER);
    reg<ModulatedDelayEffect>(effects, DspModuleType::MODULATED_DELAY_LINE);
    reg<SimpleDelayEffect>(effects, DspModuleType::SIMPLE_DELAY);
    reg<DynamicRingBufferEffect>(effects, DspModuleType::DYNAMIC_RING_BUFFER);
    reg<TapeStopEchoEffect>(effects, DspModuleType::TAPE_STOP_REVERSE_ECHO);
    reg<GranularDelayEffect>(effects, DspModuleType::GRANULAR_DELAY);
    reg<DiffusedReverbEffect>(effects, DspModuleType::DIFFUSED_DELAY_NETWORK);
    reg<PlateReverbEffect>(effects, DspModuleType::PLATE_REVERB);
    reg<GranularPitchEffect>(effects, DspModuleType::PITCH_SHIFTER_GRANULAR);
    reg<FrequencyShifterEffect>(effects, DspModuleType::FREQUENCY_SHIFTER);
    reg<SubSynthEffect>(effects, DspModuleType::PITCH_DETECTOR_OSCILLATOR);
    reg<VcaCompressorEffect>(effects, DspModuleType::ENVELOPE_VCA_COMPRESSOR);
    reg<SampleRateDegraderEffect>(effects, DspModuleType::SAMPLE_RATE_DEGRADER);
}

}

// -------------------------------------------------------------------------

UnifiedPedalProcessor::UnifiedPedalProcessor()
    : m_sampleRate(44100.0),
      m_maxSamplesPerBlock(1024),
      m_maxChannels(2),
      m_crossfadeSamples(882)  // Default: 20ms at 44.1kHz
{
    registerAllEffects(m_effects);

    // Initialize parameter cache to default values
    for (auto& param : m_parameterCache)
        param.store(0.5f, std::memory_order_relaxed);
}

void UnifiedPedalProcessor::prepareToPlay(double sampleRate, int maxSamplesPerBlock, int numChannels)
{
    m_sampleRate = sampleRate;
    m_maxSamplesPerBlock = maxSamplesPerBlock;
    m_maxChannels = std::max(1, numChannels);

    // Calculate crossfade samples from time (sample-rate independent)
    m_crossfadeSamples = static_cast<int>(kCrossfadeMs * sampleRate / 1000.0);
    m_crossfadeSamples = std::max(1, m_crossfadeSamples);  // At least 1 sample

    // Preallocate all buffers - no allocations during processBlock
    const size_t maxSamples = static_cast<size_t>(maxSamplesPerBlock);
    const size_t maxCh = static_cast<size_t>(m_maxChannels);

    m_crossfadeTempBuf.resize(maxCh);
    for (auto& buf : m_crossfadeTempBuf)
        buf.assign(maxSamples, 0.0f);

    m_dryBuffer.assign(maxCh, 0.0f);
    m_crossfadeOldOut.assign(maxCh, 0.0f);

    for (auto& effect : m_effects)
        if (effect)
            effect->prepare(sampleRate, numChannels);

    reset();
}

void UnifiedPedalProcessor::reset()
{
    for (auto& effect : m_effects)
        if (effect)
            effect->reset();

    m_crossfadeCounter = 0;
}

void UnifiedPedalProcessor::loadPedalConfiguration(std::shared_ptr<PedalAssetPayload> config)
{
    if (!config) return;

    // Thread-safe atomic swap: UI/compiler thread writes, audio thread only reads
    auto current = m_currentConfig.load();
    if (current && !current->activeRoutingChain.empty())
    {
        m_nextConfig.store(std::move(config));
        m_crossfadeCounter = 0;
    }
    else
    {
        m_currentConfig.store(std::move(config));
        reset();
    }
}

float UnifiedPedalProcessor::readParam(uint16_t token, float fallback) const
{
    if (!m_activeConfig) return fallback;
    for (auto& p : m_activeConfig->parameters)
        if (p.parameterToken == token && p.targetDspNodeRegister == static_cast<uint8_t>(m_currentNodeIndex))
            return p.currentValue;
    return fallback;
}

std::vector<ParameterDescriptor> UnifiedPedalProcessor::getCurrentParams() const
{
    auto current = m_currentConfig.load();
    if (current)
        return current->parameters;
    return {};
}

std::shared_ptr<PedalAssetPayload> UnifiedPedalProcessor::getCurrentConfig() const
{
    return m_currentConfig.load();
}

void UnifiedPedalProcessor::updateParameter(int physicalSlot, int knobIdx, float newValue)
{
    // Lock-free atomic write for audio thread consumption
    if (physicalSlot >= 0 && physicalSlot < PedalSlotCount && knobIdx >= 0 && knobIdx < 4)
    {
        const size_t idx = static_cast<size_t>(physicalSlot * 4 + knobIdx);
        m_parameterCache[idx].store(newValue, std::memory_order_release);
        m_paramRevision.fetch_add(1, std::memory_order_acq_rel);
    }
}

UnifiedPedalProcessor::ParameterSnapshot UnifiedPedalProcessor::getSnapshot() const
{
    ParameterSnapshot snap;
    snap.revision = m_paramRevision.load(std::memory_order_acquire);

    for (size_t i = 0; i < snap.values.size(); ++i)
        snap.values[i] = m_parameterCache[i].load(std::memory_order_acquire);

    return snap;
}

void UnifiedPedalProcessor::processWithConfig(float** b, int c, int s, const PedalAssetPayload& config)
{
    int chCount = std::min(c, m_maxChannels);

    for (int idx = 0; idx < static_cast<int>(config.activeRoutingChain.size()); ++idx)
    {
        auto& node = config.activeRoutingChain[idx];
        m_currentNodeIndex = idx;

        for (int ch = 0; ch < chCount; ++ch)
            m_dryBuffer[static_cast<size_t>(ch)] = b[ch][s];

        auto& effect = m_effects[static_cast<int>(node)];
        if (effect)
        {
            float effectParam = readParam(static_cast<uint16_t>(ParamToken::Effect), 0.5f);
            effect->processSample(b, c, s, effectParam);
        }

        float wet = readParam(static_cast<uint16_t>(ParamToken::Wet), 0.5f);
        float dryLevel = readParam(static_cast<uint16_t>(ParamToken::Dry), 0.5f);
        float volume = readParam(static_cast<uint16_t>(ParamToken::Volume), 1.0f);
        for (int ch = 0; ch < chCount; ++ch)
            b[ch][s] = (m_dryBuffer[static_cast<size_t>(ch)] * dryLevel + b[ch][s] * wet) * volume;
    }
}

void UnifiedPedalProcessor::processAudioBlock(float** buffer, int numChannels, int numSamples)
{
    // Thread-safe: Atomic load of config pointers
    std::shared_ptr<PedalAssetPayload> current = m_currentConfig.load();
    std::shared_ptr<PedalAssetPayload> next = m_nextConfig.load();

    if (!current)
        return;

    // Use preallocated buffers - no reallocation during processing
    // Note: buffers should be pre-sized in prepareToPlay

    m_activeConfig = current.get();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (next && m_crossfadeCounter < m_crossfadeSamples)
        {
            int crossCh = std::min(numChannels, m_maxChannels);
            for (int ch = 0; ch < crossCh; ++ch)
                m_crossfadeTempBuf[static_cast<size_t>(ch)][static_cast<size_t>(sample)] = buffer[ch][sample];

            m_activeConfig = current.get();
            processWithConfig(buffer, numChannels, sample, *current);

            for (int ch = 0; ch < crossCh; ++ch)
                m_crossfadeOldOut[static_cast<size_t>(ch)] = buffer[ch][sample];

            for (int ch = 0; ch < crossCh; ++ch)
                buffer[ch][sample] = m_crossfadeTempBuf[static_cast<size_t>(ch)][static_cast<size_t>(sample)];

            m_activeConfig = next.get();
            processWithConfig(buffer, numChannels, sample, *next);

            float g = static_cast<float>(m_crossfadeCounter) / static_cast<float>(m_crossfadeSamples);
            for (int ch = 0; ch < crossCh; ++ch)
                buffer[ch][sample] = m_crossfadeOldOut[static_cast<size_t>(ch)] * (1.0f - g) + buffer[ch][sample] * g;

            ++m_crossfadeCounter;
        }
        else
        {
            m_activeConfig = current.get();
            processWithConfig(buffer, numChannels, sample, *current);
        }
    }

    m_activeConfig = nullptr;

    // Thread-safe config commit (atomic swap)
    if (next && m_crossfadeCounter >= m_crossfadeSamples)
    {
        m_currentConfig.store(next);
        m_nextConfig.store(nullptr);
        m_crossfadeCounter = 0;
        reset();
    }
}
