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
        defaultKnobLayout(),
        {{
            { ParamToken::Knob0, l0, 0.0f, 1.0f, 0.5f },
            { ParamToken::Knob1, l1, 0.0f, 1.0f, 0.5f },
            { ParamToken::Knob2, l2, 0.0f, 1.0f, 0.5f },
            { ParamToken::Knob3, l3, 0.0f, 1.0f, 0.5f }
        }}
    };
}

const std::array<PedalDefinition, 27>& definitions()
{
    static_assert(27 == static_cast<size_t>(DspModuleType::ANALOG_OCTAVER) + 1,
                  "PedalDefinitions array must match DspModuleType enum count");
    static const std::array<PedalDefinition, 27> defs {{
makeDefinition(DspModuleType::BYPASS, "Bypass",             "Mix",    "",       "",       ""),
        makeDefinition(DspModuleType::WAVESHAPER_DISTORTION, "Waveshaper",   "",       "",       "Drive",  ""),
        makeDefinition(DspModuleType::MICROPITCH_CHORUS,     "MicroPitch",  "Mix",    "Depth", "Detune", "Rate"),
        makeDefinition(DspModuleType::MULTI_MODE_FILTER,     "Multi Filter",  "Mode",   "",       "Cutoff", ""),
        makeDefinition(DspModuleType::PITCH_SHIFTER_GRANULAR,"Pitch Shifter",      "",       "",       "Rate",   ""),
        makeDefinition(DspModuleType::ENVELOPE_VCA_COMPRESSOR,"VCA Compressor",    "Attack", "Release","Thresh", "Level"),
        makeDefinition(DspModuleType::GLITCH_STUTTER,        "Glitch Stutter",     "Intens", "",       "",       ""),
        makeDefinition(DspModuleType::DIFFUSED_DELAY_NETWORK,"Diff. Delay Net",    "",       "",       "",       "Decay"),
        makeDefinition(DspModuleType::MATHEMATICAL_WAVEFOLDER,"Wavefolder",        "",       "Fold",  "",       ""),
        makeDefinition(DspModuleType::FORMANT_VOCAL_SHIFTER, "Formant Shifter",    "Q",      "Shift", "Formant","Level"),
        makeDefinition(DspModuleType::TAPE_STOP_REVERSE_ECHO,"Tape Stop Echo",     "",       "Brake", "",       ""),
        makeDefinition(DspModuleType::SIMPLE_DELAY,          "Simple Delay",       "Mix",    "Time",  "Feed",   "Damp"),
        makeDefinition(DspModuleType::PLATE_REVERB,          "Plate Reverb",       "",       "",      "Decay",  ""),
        makeDefinition(DspModuleType::RHYTHM_GATE,              "Rhythm Gate",        "Rate",   "Shape", "Depth",  "Mix"),
        makeDefinition(DspModuleType::GRANULAR_DELAY,        "Granular Delay",     "Mix",    "Spread","Size",   "Rate"),
        makeDefinition(DspModuleType::COMB_RESONATOR,        "Comb Resonator",     "Freq",   "",      "",       ""),
        makeDefinition(DspModuleType::SPECTRAL_FREEZE,       "Spectral Freeze",    "",       "Freeze","",       ""),
        makeDefinition(DspModuleType::FREQ_SHIFTER,          "Frequency Shift",    "Shift",  "",      "",       ""),
        makeDefinition(DspModuleType::REVERSE_BUFFER,        "Reverse Buffer",     "",       "",      "",       "Density"),
        makeDefinition(DspModuleType::GRAIN_SCRUBBER,        "Grain Scrubber",     "Pos",    "",      "",       "Level"),
        makeDefinition(DspModuleType::SPECTRAL_FILTER,       "Spectral Filter",    "Width",  "Center","Q",      "Level"),
        makeDefinition(DspModuleType::CONVOLUTION_SPACE,     "Conv Space",  "",       "",      "",       "Damp"),
        makeDefinition(DspModuleType::RANDOM_MODULATOR,      "Random Modulator",   "Depth",  "Smooth","Rate",   ""),
        makeDefinition(DspModuleType::RESAMPLE_BITCRUSH,     "Resampler",          "Rate",   "Bits",  "Dither", "Filter"),
        makeDefinition(DspModuleType::TREMOLO,               "Tremolo",            "Mix",    "Rate",  "Depth",  "Shape"),
        makeDefinition(DspModuleType::FLANGER,               "Flanger",            "Mix",    "Rate",  "Depth",  "Feed"),
        makeDefinition(DspModuleType::ANALOG_OCTAVER,        "Octaver",            "Mix",    "Sub",   "Upper",  "Tone"),
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
}
