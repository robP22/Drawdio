#include "DspEffectFactory.h"
#include "Effects/DistortionEffects.h"
#include "Effects/FilterEffects.h"
#include "Effects/DelayEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/MiscEffects.h"
#include "Effects/ReverseEffect.h"
#include "Effects/GrainScrubberEffect.h"
#include "Effects/SpectralFilterEffect.h"
#include "Effects/ConvolutionReverbEffect.h"
#include "Effects/BitcrusherEffect.h"
#include "Effects/ModulationEffects.h"
#include "Effects/ReTimeEffect.h"
#include "Effects/HpLpFilterEffect.h"

std::unique_ptr<DspEffect> createDspEffect(DspModuleType type)
{
    switch (type)
    {
        case DspModuleType::WAVESHAPER:               return std::make_unique<WaveshaperEffect>();
        case DspModuleType::CHORUS:                   return std::make_unique<ChorusEffect>();
        case DspModuleType::MULTI_MODE_FILTER:        return std::make_unique<MultiModeFilterEffect>();
        case DspModuleType::PITCH_SHIFTER:            return std::make_unique<PitchShifterEffect>();
        case DspModuleType::VCA_COMPRESSOR:           return std::make_unique<VcaCompressorEffect>();
        case DspModuleType::GLITCH_STUTTER:           return std::make_unique<GlitchStutterEffect>();
        case DspModuleType::DIFFUSED_REVERB:          return std::make_unique<DiffusedReverbEffect>();
        case DspModuleType::WAVEFOLDER:               return std::make_unique<WavefolderEffect>();
        case DspModuleType::FORMANT_SHIFTER:          return std::make_unique<FormantShifterEffect>();
        case DspModuleType::RETIME:                   return std::make_unique<ReTimeEffect>();
        case DspModuleType::DELAY:                    return std::make_unique<DelayEffect>();
        case DspModuleType::PLATE_REVERB:             return std::make_unique<PlateReverbEffect>();
        case DspModuleType::SIDECHAIN:                return std::make_unique<SidechainEffect>();
        case DspModuleType::GRANULAR_DELAY:           return std::make_unique<GranularDelayEffect>();
        case DspModuleType::COMB_RESONATOR:           return std::make_unique<CombResonatorEffect>();
        case DspModuleType::SPECTRAL_FREEZE:          return std::make_unique<SpectralFreezeEffect>();
        case DspModuleType::FREQ_SHIFTER:             return std::make_unique<FrequencyShifterEffect>();
        case DspModuleType::REVERSE:                  return std::make_unique<ReverseEffect>();
        case DspModuleType::GRAIN_SCRUBBER:           return std::make_unique<GrainScrubberEffect>();
        case DspModuleType::SPECTRAL_FILTER:          return std::make_unique<SpectralFilterEffect>();
        case DspModuleType::CONVOLUTION_REVERB:       return std::make_unique<ConvolutionReverbEffect>();
        case DspModuleType::HP_LP_FILTER:             return std::make_unique<HpLpFilterEffect>();
        case DspModuleType::BITCRUSHER:               return std::make_unique<BitcrusherEffect>();
        case DspModuleType::TREMOLO:                  return std::make_unique<TremoloEffect>();
        case DspModuleType::FLANGER:                  return std::make_unique<FlangerEffect>();
        default:                                      return nullptr;
    }
}
