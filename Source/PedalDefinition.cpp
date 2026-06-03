#include "PedalDefinition.h"

namespace
{
constexpr auto defaultKnobLayout()
{
    return std::array<NormalizedControlBounds, 4> {{
        { 0.31f, 0.35f, 0.34f, 0.22f },
        { 0.69f, 0.35f, 0.34f, 0.22f },
        { 0.31f, 0.56f, 0.34f, 0.22f },
        { 0.69f, 0.56f, 0.34f, 0.22f }
    }};
}

PedalDefinition makeDefinition(DspModuleType type, const char* displayName, const char* effectLabel)
{
    return {
        type,
        displayName,
        {},
        defaultKnobLayout(),
        {{
            { ParamToken::Wet, "Wet", 0.0f, 1.0f, 0.5f },
            { ParamToken::Dry, "Dry", 0.0f, 1.0f, 0.5f },
            { ParamToken::Volume, "Vol", 0.0f, 1.0f, 0.5f },
            { ParamToken::Effect, effectLabel, 0.0f, 1.0f, 0.5f }
        }}
    };
}

const std::array<PedalDefinition, static_cast<size_t>(DspModuleType::GRANULAR_DELAY) + 1>& definitions()
{
    static const std::array<PedalDefinition, static_cast<size_t>(DspModuleType::GRANULAR_DELAY) + 1> defs {{
        makeDefinition(DspModuleType::BYPASS, "Bypass", "Param"),
        makeDefinition(DspModuleType::WAVESHAPER_DISTORTION, "Waveshaper Dist.", "Drive"),
        makeDefinition(DspModuleType::MODULATED_DELAY_LINE, "Mod. Delay", "Rate"),
        makeDefinition(DspModuleType::BIQUAD_FILTER, "Biquad Filter", "Cutoff"),
        makeDefinition(DspModuleType::DYNAMIC_RING_BUFFER, "Dyn. Ring Buffer", "Size"),
        makeDefinition(DspModuleType::PITCH_SHIFTER_GRANULAR, "Pitch Shifter", "Pitch"),
        makeDefinition(DspModuleType::ENVELOPE_VCA_COMPRESSOR, "VCA Compressor", "Thresh"),
        makeDefinition(DspModuleType::PITCH_DETECTOR_OSCILLATOR, "Pitch Detector", "Octave"),
        makeDefinition(DspModuleType::DIFFUSED_DELAY_NETWORK, "Diff. Delay Net", "Decay"),
        makeDefinition(DspModuleType::ALLPASS_FILTER_CASCADE, "Allpass Cascade", "Coeff"),
        makeDefinition(DspModuleType::FREQUENCY_SHIFTER, "Freq. Shifter", "Shift"),
        makeDefinition(DspModuleType::MATHEMATICAL_WAVEFOLDER, "Wavefolder", "Fold"),
        makeDefinition(DspModuleType::SAMPLE_RATE_DEGRADER, "Sample Rate Deg.", "Bits"),
        makeDefinition(DspModuleType::FORMANT_VOCAL_SHIFTER, "Formant Shifter", "Formant"),
        makeDefinition(DspModuleType::TAPE_STOP_REVERSE_ECHO, "Tape Stop Echo", "Brake"),
        makeDefinition(DspModuleType::SIMPLE_DELAY, "Simple Delay", "Time"),
        makeDefinition(DspModuleType::PLATE_REVERB, "Plate Reverb", "Decay"),
        makeDefinition(DspModuleType::SOFT_DISTORTION, "Soft Distortion", "Drive"),
        makeDefinition(DspModuleType::GRANULAR_DELAY, "Gran. Delay", "Pitch")
    }};

    return defs;
}
}

namespace PedalDefinitions
{
const PedalDefinition& get(DspModuleType type)
{
    const auto index = static_cast<size_t>(type);
    const auto& defs = definitions();
    if (index < defs.size())
        return defs[index];

    return fallback();
}

const PedalDefinition& fallback()
{
    return definitions()[0];
}

juce::String getDisplayName(DspModuleType type)
{
    return get(type).displayName;
}

juce::String getParameterLabel(DspModuleType type, int parameterIndex)
{
    const auto& definition = get(type);
    if (parameterIndex >= 0 && parameterIndex < static_cast<int>(definition.parameters.size()))
        return definition.parameters[static_cast<size_t>(parameterIndex)].label;

    return "Param";
}
}
