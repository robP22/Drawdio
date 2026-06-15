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

PedalDefinition makeDefinition(DspModuleType type, const char* displayName,
                                const char* param2Label, const char* effectLabel)
{
    return {
        type,
        displayName,
        {},
        defaultKnobLayout(),
        {{
            { ParamToken::Wet, "Wet", 0.0f, 1.0f, 0.5f },
            { ParamToken::Dry, "Dry", 0.0f, 1.0f, 0.5f },
            { ParamToken::Volume, param2Label, 0.0f, 1.0f, 0.5f },
            { ParamToken::Effect, effectLabel, 0.0f, 1.0f, 0.5f }
        }}
    };
}

const std::array<PedalDefinition, static_cast<size_t>(DspModuleType::GRANULAR_DELAY) + 1>& definitions()
{
    static const std::array<PedalDefinition, static_cast<size_t>(DspModuleType::GRANULAR_DELAY) + 1> defs {{
        makeDefinition(DspModuleType::BYPASS, "Bypass", "Vol", "Param"),
        makeDefinition(DspModuleType::WAVESHAPER_DISTORTION, "Waveshaper Dist.", "Tone", "Drive"),
        makeDefinition(DspModuleType::MICROPITCH_CHORUS, "MicroPitch Chorus", "Depth", "Detune"),
        makeDefinition(DspModuleType::BIQUAD_FILTER, "Biquad Filter", "Res", "Cutoff"),
        makeDefinition(DspModuleType::DYNAMIC_RING_BUFFER, "Dyn. Ring Buffer", "Feed", "Size"),
        makeDefinition(DspModuleType::PITCH_SHIFTER_GRANULAR, "Pitch Shifter", "Spread", "Rate"),
        makeDefinition(DspModuleType::ENVELOPE_VCA_COMPRESSOR, "VCA Compressor", "Attack", "Thresh"),
        makeDefinition(DspModuleType::GLITCH_STUTTER, "Glitch Stutter", "Mix", "Intensity"),
        makeDefinition(DspModuleType::DIFFUSED_DELAY_NETWORK, "Diff. Delay Net", "Diff", "Decay"),
        makeDefinition(DspModuleType::SPECTRAL_FREEZE, "Spectral Freeze", "Morph", "Freeze"),
        makeDefinition(DspModuleType::FREQUENCY_SHIFTER, "Freq. Shifter", "Mix", "Shift"),
        makeDefinition(DspModuleType::MATHEMATICAL_WAVEFOLDER, "Wavefolder", "Sym", "Fold"),
        makeDefinition(DspModuleType::SAMPLE_RATE_DEGRADER, "Sample Rate Deg.", "Rate", "Bits"),
        makeDefinition(DspModuleType::FORMANT_VOCAL_SHIFTER, "Formant Shifter", "Q", "Formant"),
        makeDefinition(DspModuleType::TAPE_STOP_REVERSE_ECHO, "Tape Stop Echo", "Decay", "Brake"),
        makeDefinition(DspModuleType::SIMPLE_DELAY, "Simple Delay", "Feed", "Time"),
        makeDefinition(DspModuleType::PLATE_REVERB, "Plate Reverb", "Size", "Decay"),
        makeDefinition(DspModuleType::COMB_RESONATOR, "Comb Resonator", "Feed", "Freq"),
        makeDefinition(DspModuleType::GRANULAR_DELAY, "Gran. Delay", "Spread", "Rate")
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
