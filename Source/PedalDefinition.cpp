#include "PedalDefinition.h"

namespace
{
constexpr auto defaultKnobLayout()
{
    return std::array<NormalizedControlBounds, 4> {{
        { 0.31f, 0.345f, 0.34f, 0.22f },
        { 0.69f, 0.345f, 0.34f, 0.22f },
        { 0.31f, 0.565f, 0.34f, 0.22f },
        { 0.69f, 0.565f, 0.34f, 0.22f }
    }};
}

PedalDefinition makeDefinition(DspModuleType type, const char* displayName,
                                const char* l0, const char* l1,
                                const char* l2, const char* l3)
{
    return {
        type,
        displayName,
        {},
        defaultKnobLayout(),
        {{
            { ParamToken::Knob0, l0, 0.0f, 1.0f, 0.5f },
            { ParamToken::Knob1, l1, 0.0f, 1.0f, 0.5f },
            { ParamToken::Knob2, l2, 0.0f, 1.0f, 0.5f },
            { ParamToken::Knob3, l3, 0.0f, 1.0f, 0.5f }
        }}
    };
}

const std::array<PedalDefinition, 23>& definitions()
{
    static const std::array<PedalDefinition, 23> defs {{
makeDefinition(DspModuleType::BYPASS, "Bypass",             "Mix",    "",       "",       ""),
        makeDefinition(DspModuleType::WAVESHAPER_DISTORTION, "Waveshaper",   "Tone",   "Sym",   "Drive",  "Level"),
        makeDefinition(DspModuleType::MICROPITCH_CHORUS,     "MicroPitch",  "Mix",    "Depth", "Detune", "Rate"),
        makeDefinition(DspModuleType::MULTI_MODE_FILTER,     "Multi Filter",  "Mode",   "Res",   "Cutoff", "Level"),
        makeDefinition(DspModuleType::PITCH_SHIFTER_GRANULAR,"Pitch Shifter",      "Spread", "Grain", "Rate",   "Level"),
        makeDefinition(DspModuleType::ENVELOPE_VCA_COMPRESSOR,"VCA Compressor",    "Attack", "Release","Thresh", "Level"),
        makeDefinition(DspModuleType::GLITCH_STUTTER,        "Glitch Stutter",     "Intens", "Gate",  "Rate",   "Level"),
        makeDefinition(DspModuleType::DIFFUSED_DELAY_NETWORK,"Diff. Delay Net",    "Mix",    "Diff",  "Size",   "Decay"),
        makeDefinition(DspModuleType::MATHEMATICAL_WAVEFOLDER,"Wavefolder",        "Sym",    "Fold",  "Drive",  "Level"),
        makeDefinition(DspModuleType::FORMANT_VOCAL_SHIFTER, "Formant Shifter",    "Q",      "Shift", "Formant","Level"),
        makeDefinition(DspModuleType::TAPE_STOP_REVERSE_ECHO,"Tape Stop Echo",     "Mix",    "Brake", "Speed",  "Decay"),
        makeDefinition(DspModuleType::SIMPLE_DELAY,          "Simple Delay",       "Mix",    "Time",  "Feed",   "Damp"),
        makeDefinition(DspModuleType::PLATE_REVERB,          "Plate Reverb",       "Mix",    "Size",  "Decay",  "Damp"),
        makeDefinition(DspModuleType::SIDECHAIN_DUCKER,      "Sidechain Pump",     "Rate",   "Shape", "Amount", "Level"),
        makeDefinition(DspModuleType::GRANULAR_DELAY,        "Granular Delay",     "Mix",    "Spread","Size",   "Rate"),
        makeDefinition(DspModuleType::COMB_RESONATOR,        "Comb Resonator",     "Freq",   "Feed",  "Decay",  "Level"),
        makeDefinition(DspModuleType::SPECTRAL_FREEZE,       "Spectral Freeze",    "Mix",    "Freeze","Drift",  "Window"),
        makeDefinition(DspModuleType::FREQ_SHIFTER,          "Frequency Shift",    "Shift",  "Spread","Depth",  "Level"),
        makeDefinition(DspModuleType::REVERSE_BUFFER,        "Reverse Buffer",     "Mix",    "Length","Dir",    "Density"),
        makeDefinition(DspModuleType::GRAIN_SCRUBBER,        "Grain Scrubber",     "Pos",    "Density","Size",  "Level"),
        makeDefinition(DspModuleType::SPECTRAL_FILTER,       "Spectral Filter",    "Width",  "Center","Q",      "Level"),
        makeDefinition(DspModuleType::CONVOLUTION_SPACE,     "Conv Space",  "Mix",    "Space", "Size",   "Damp"),
        makeDefinition(DspModuleType::RANDOM_MODULATOR,      "Random Modulator",   "Depth",  "Smooth","Rate",   "Shape"),
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
