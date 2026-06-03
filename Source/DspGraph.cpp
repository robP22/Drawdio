#include "DspGraph.h"
#include "Effects/DistortionEffects.h"
#include "Effects/FilterEffects.h"
#include "Effects/DelayEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/MiscEffects.h"

namespace
{
template<typename T>
std::unique_ptr<DspEffect> createEffect()
{
    return std::make_unique<T>();
}

std::unique_ptr<DspEffect> createEffectForType(DspModuleType type)
{
    switch (type)
    {
        case DspModuleType::BYPASS: return nullptr;
        case DspModuleType::WAVESHAPER_DISTORTION: return createEffect<WaveshaperEffect>();
        case DspModuleType::SOFT_DISTORTION: return createEffect<SoftDistortionEffect>();
        case DspModuleType::MATHEMATICAL_WAVEFOLDER: return createEffect<WavefolderEffect>();
        case DspModuleType::BIQUAD_FILTER: return createEffect<BiquadFilterEffect>();
        case DspModuleType::ALLPASS_FILTER_CASCADE: return createEffect<AllpassCascadeEffect>();
        case DspModuleType::FORMANT_VOCAL_SHIFTER: return createEffect<FormantShifterEffect>();
        case DspModuleType::MODULATED_DELAY_LINE: return createEffect<ModulatedDelayEffect>();
        case DspModuleType::SIMPLE_DELAY: return createEffect<SimpleDelayEffect>();
        case DspModuleType::DYNAMIC_RING_BUFFER: return createEffect<DynamicRingBufferEffect>();
        case DspModuleType::TAPE_STOP_REVERSE_ECHO: return createEffect<TapeStopEchoEffect>();
        case DspModuleType::GRANULAR_DELAY: return createEffect<GranularDelayEffect>();
        case DspModuleType::DIFFUSED_DELAY_NETWORK: return createEffect<DiffusedReverbEffect>();
        case DspModuleType::PLATE_REVERB: return createEffect<PlateReverbEffect>();
        case DspModuleType::PITCH_SHIFTER_GRANULAR: return createEffect<GranularPitchEffect>();
        case DspModuleType::FREQUENCY_SHIFTER: return createEffect<FrequencyShifterEffect>();
        case DspModuleType::PITCH_DETECTOR_OSCILLATOR: return createEffect<SubSynthEffect>();
        case DspModuleType::ENVELOPE_VCA_COMPRESSOR: return createEffect<VcaCompressorEffect>();
        case DspModuleType::SAMPLE_RATE_DEGRADER: return createEffect<SampleRateDegraderEffect>();
        default: return nullptr;
    }
}
}

DspGraph::DspGraph()
{
    for (auto& effect : m_effects)
        effect.reset();
    for (auto& effect : m_nextEffects)
        effect.reset();
}

void DspGraph::buildChain(const std::vector<DspModuleType>& effectTypes)
{
    m_chainLength = 0;

    for (size_t i = 0; i < effectTypes.size() && i < static_cast<size_t>(MaxChainNodes); ++i)
    {
        m_effects[static_cast<size_t>(i)] = createEffectForType(effectTypes[i]);
        if (m_effects[static_cast<size_t>(i)])
            ++m_chainLength;
    }
}

void DspGraph::prepare(double sampleRate, int numChannels)
{
    for (int i = 0; i < m_chainLength; ++i)
    {
        if (m_effects[static_cast<size_t>(i)])
            m_effects[static_cast<size_t>(i)]->prepare(sampleRate, numChannels);
    }
    for (int i = 0; i < MaxChainNodes; ++i)
    {
        if (m_nextEffects[static_cast<size_t>(i)])
            m_nextEffects[static_cast<size_t>(i)]->prepare(sampleRate, numChannels);
    }
}

void DspGraph::reset()
{
    for (int i = 0; i < m_chainLength; ++i)
    {
        if (m_effects[static_cast<size_t>(i)])
            m_effects[static_cast<size_t>(i)]->reset();
    }
    m_crossfadeRemaining = 0;
}

void DspGraph::process(float** buffer, int numChannels, int numSamples)
{
    for (int i = 0; i < m_chainLength; ++i)
    {
        auto& effect = m_effects[static_cast<size_t>(i)];
        if (effect)
        {
            for (int s = 0; s < numSamples; ++s)
                effect->processSample(buffer, numChannels, s, 0.5f);
        }
    }
}