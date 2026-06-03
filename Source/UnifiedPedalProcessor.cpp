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
      m_maxChannels(2),
      m_crossfadeCounter(0),
      m_currentNodeIndex(0)
{
    registerAllEffects(m_effects);
}

void UnifiedPedalProcessor::prepareToPlay(double sampleRate, int maxSamplesPerBlock, int numChannels)
{
    m_sampleRate = sampleRate;
    m_maxChannels = std::max(1, numChannels);

    m_crossfadeTempBuf.resize(static_cast<size_t>(m_maxChannels));
    for (auto& buf : m_crossfadeTempBuf)
        buf.resize(static_cast<size_t>(maxSamplesPerBlock));

    m_dryBuffer.resize(static_cast<size_t>(m_maxChannels));
    m_crossfadeOldOut.resize(static_cast<size_t>(m_maxChannels));

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
    std::unique_lock<std::shared_mutex> lock(m_dspMutex);

    if (m_currentConfig && !m_currentConfig->activeRoutingChain.empty())
    {
        m_nextConfig = std::move(config);
        m_crossfadeCounter = 0;
    }
    else
    {
        m_currentConfig = std::move(config);
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
    std::shared_lock<std::shared_mutex> lock(m_dspMutex);
    if (m_currentConfig)
        return m_currentConfig->parameters;
    return {};
}

std::shared_ptr<PedalAssetPayload> UnifiedPedalProcessor::getCurrentConfig() const
{
    std::shared_lock<std::shared_mutex> lock(m_dspMutex);
    return m_currentConfig;
}

void UnifiedPedalProcessor::updateParameter(int physicalSlot, int knobIdx, float newValue)
{
    std::unique_lock<std::shared_mutex> lock(m_dspMutex);
    if (!m_currentConfig) return;

    int chainPos = -1;
    for (int i = 0; i < static_cast<int>(m_currentConfig->routingSlotOrder.size()); ++i)
    {
        if (m_currentConfig->routingSlotOrder[i] == static_cast<uint8_t>(physicalSlot))
        {
            chainPos = i;
            break;
        }
    }

    if (chainPos != -1)
    {
        for (auto& p : m_currentConfig->parameters)
        {
            if (p.targetDspNodeRegister == static_cast<uint8_t>(chainPos) &&
                p.parameterToken == static_cast<uint16_t>(knobIdx))
            {
                p.currentValue = newValue;
                p.isManualOverride = true;
                break;
            }
        }
    }
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
    // --- Snapshot config pointers under mutex (microseconds) ---
    std::shared_ptr<PedalAssetPayload> current, next;

    {
        std::shared_lock<std::shared_mutex> lock(m_dspMutex);
        if (m_currentConfig)
            current = m_currentConfig;
        if (m_nextConfig && m_crossfadeCounter < kCrossfadeLength)
            next = m_nextConfig;
    }

    if (!current)
        return;

    // Ensure scratch buffers are adequately sized (handle hosts that skip prepareToPlay).
    auto neededCh = static_cast<size_t>(m_maxChannels);
    if (m_dryBuffer.size() < neededCh)
        m_dryBuffer.resize(neededCh);
    if (m_crossfadeOldOut.size() < neededCh)
        m_crossfadeOldOut.resize(neededCh);

    // --- Processing phase (no mutex held) ---
    m_activeConfig = current.get();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (next && m_crossfadeCounter < kCrossfadeLength)
        {
            int crossCh = std::min(numChannels, m_maxChannels);
            for (int ch = 0; ch < crossCh; ++ch)
                m_crossfadeTempBuf[static_cast<size_t>(ch)][sample] = buffer[ch][sample];

            m_activeConfig = current.get();
            processWithConfig(buffer, numChannels, sample, *current);

            for (int ch = 0; ch < crossCh; ++ch)
                m_crossfadeOldOut[static_cast<size_t>(ch)] = buffer[ch][sample];

            for (int ch = 0; ch < crossCh; ++ch)
                buffer[ch][sample] = m_crossfadeTempBuf[static_cast<size_t>(ch)][sample];

            m_activeConfig = next.get();
            processWithConfig(buffer, numChannels, sample, *next);

            float g = static_cast<float>(m_crossfadeCounter) / static_cast<float>(kCrossfadeLength);
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

    // --- Commit crossfade completion under mutex (microseconds) ---
    if (next && m_crossfadeCounter >= kCrossfadeLength)
    {
        std::unique_lock<std::shared_mutex> lock(m_dspMutex);
        m_currentConfig = std::move(m_nextConfig);
        m_nextConfig.reset();
        m_crossfadeCounter = 0;
        reset();
    }
}
