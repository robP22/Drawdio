#include "DspEffectFactory.h"
#include "Effects/DistortionEffects.h"
#include "Effects/FilterEffects.h"
#include "Effects/DelayEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/MiscEffects.h"
#include "Effects/ReverseBufferEffect.h"
#include "Effects/GrainScrubberEffect.h"
#include "Effects/SpectralFilterEffect.h"
#include "Effects/ConvolutionSpaceEffect.h"
#include "Effects/RandomModulatorEffect.h"

std::unique_ptr<DspEffect> createDspEffect(DspModuleType type)
{
    switch (type)
    {
        case DspModuleType::WAVESHAPER_DISTORTION:   return std::make_unique<WaveshaperEffect>();
        case DspModuleType::MICROPITCH_CHORUS:        return std::make_unique<MicroPitchChorusEffect>();
        case DspModuleType::MULTI_MODE_FILTER:        return std::make_unique<MultiModeFilterEffect>();
        case DspModuleType::PITCH_SHIFTER_GRANULAR:   return std::make_unique<GranularPitchEffect>();
        case DspModuleType::ENVELOPE_VCA_COMPRESSOR:  return std::make_unique<VcaCompressorEffect>();
        case DspModuleType::GLITCH_STUTTER:           return std::make_unique<GlitchStutterEffect>();
        case DspModuleType::DIFFUSED_DELAY_NETWORK:   return std::make_unique<DiffusedReverbEffect>();
        case DspModuleType::MATHEMATICAL_WAVEFOLDER:  return std::make_unique<WavefolderEffect>();
        case DspModuleType::FORMANT_VOCAL_SHIFTER:    return std::make_unique<DynamicResonantFilter>();
        case DspModuleType::TAPE_STOP_REVERSE_ECHO:   return std::make_unique<TapeStopEchoEffect>();
        case DspModuleType::SIMPLE_DELAY:             return std::make_unique<SimpleDelayEffect>();
        case DspModuleType::PLATE_REVERB:             return std::make_unique<PlateReverbEffect>();
        case DspModuleType::SIDECHAIN_DUCKER:         return std::make_unique<SidechainDuckerEffect>();
        case DspModuleType::GRANULAR_DELAY:           return std::make_unique<GranularDelayEffect>();
        case DspModuleType::COMB_RESONATOR:           return std::make_unique<CombResonatorEffect>();
        case DspModuleType::SPECTRAL_FREEZE:          return std::make_unique<TimeDomainFreezeEffect>();
        case DspModuleType::FREQ_SHIFTER:             return std::make_unique<FrequencyShifterEffect>();
        case DspModuleType::REVERSE_BUFFER:           return std::make_unique<ReverseBufferEffect>();
        case DspModuleType::GRAIN_SCRUBBER:           return std::make_unique<GrainScrubberEffect>();
        case DspModuleType::SPECTRAL_FILTER:          return std::make_unique<SpectralFilterEffect>();
        case DspModuleType::CONVOLUTION_SPACE:        return std::make_unique<ConvolutionSpaceEffect>();
        case DspModuleType::RANDOM_MODULATOR:         return std::make_unique<RandomModulatorEffect>();
        default:                                      return nullptr;
    }
}
