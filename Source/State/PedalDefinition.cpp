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
    static_assert(27 == static_cast<size_t>(DspModuleType::RESERVED_REMOVED_OCTAVER) + 1,
                  "PedalDefinitions array must match DspModuleType enum count");
    static const std::array<PedalDefinition, 27> defs {{
makeDefinition(DspModuleType::BYPASS, "Bypass",             "Mix",    "",       "",       ""),
        makeDefinition(DspModuleType::WAVESHAPER_DISTORTION, "Wave Shaper",  "",       "",       "Drive",  ""),
        makeDefinition(DspModuleType::MICROPITCH_CHORUS,     "MicroPitch",  "Mix",    "Depth", "Detune", "Rate"),
        makeDefinition(DspModuleType::MULTI_MODE_FILTER,     "Multi-Mode Filter", "Mode", "",       "Cutoff", ""),
        makeDefinition(DspModuleType::PITCH_SHIFTER_GRANULAR,"Pitch Shifter",      "",       "",       "Pitch",  ""),
        makeDefinition(DspModuleType::ENVELOPE_VCA_COMPRESSOR,"VCA Compressor",    "Attack", "Release","Thresh", "Level"),
        makeDefinition(DspModuleType::GLITCH_STUTTER,        "Glitch Stutter",     "Intens", "",       "",       ""),
        makeDefinition(DspModuleType::DIFFUSED_DELAY_NETWORK,"Diffused Reverb",    "Mix",    "",       "",       "Decay"),
        makeDefinition(DspModuleType::MATHEMATICAL_WAVEFOLDER,"Wave Folder",       "",       "Fold",  "",       ""),
        makeDefinition(DspModuleType::FORMANT_VOCAL_SHIFTER, "Formant Shifter",    "Q",      "Shift", "Formant","Level"),
        makeDefinition(DspModuleType::RETIME,                "Re-Time",            "Mix",    "Speed", "Loop",   "Smooth"),
        makeDefinition(DspModuleType::SIMPLE_DELAY,          "Simple Delay",       "Mix",    "Time",  "Feed",   "Damp"),
        makeDefinition(DspModuleType::PLATE_REVERB,          "Plate Reverb",       "Mix",    "",      "Decay",  ""),
        makeDefinition(DspModuleType::RHYTHM_GATE,              "Rhythm Gate",        "Rate",   "Shape", "Depth",  "Mix"),
        makeDefinition(DspModuleType::GRANULAR_DELAY,        "Granular Delay",     "Mix",    "Spread","Size",   "Rate"),
        makeDefinition(DspModuleType::COMB_RESONATOR,        "Comb Resonator",     "Freq",   "",      "",       ""),
        makeDefinition(DspModuleType::SPECTRAL_FREEZE,       "Time Freeze",        "Mix",    "Freeze","",       ""),
        makeDefinition(DspModuleType::FREQ_SHIFTER,          "Frequency Shifter",  "Shift",  "",      "",       ""),
        makeDefinition(DspModuleType::REVERSE_BUFFER,        "Reverse Buffer",     "Mix",    "",      "",       "Density"),
        makeDefinition(DspModuleType::GRAIN_SCRUBBER,        "Grain Scrubber",     "Position","",     "",       "Rate"),
        makeDefinition(DspModuleType::SPECTRAL_FILTER,       "Resonant Filter",    "Width",  "Center","Q",      "Level"),
        makeDefinition(DspModuleType::CONVOLUTION_SPACE,     "Convolution Space",  "Mix",    "Size",  "Width",  "Damp"),
        makeDefinition(DspModuleType::RESERVED_REMOVED_RANDOM_MODULATOR, "Bypass",  "",       "",      "",       ""),
        makeDefinition(DspModuleType::RESAMPLE_BITCRUSH,     "Resampler",          "Rate",   "Bits",  "Dither", "Filter"),
        makeDefinition(DspModuleType::TREMOLO,               "Tremolo",            "Mix",    "Rate",  "Depth",  "Shape"),
        makeDefinition(DspModuleType::FLANGER,               "Flanger",            "Mix",    "Rate",  "Depth",  "Feed"),
        makeDefinition(DspModuleType::RESERVED_REMOVED_OCTAVER, "Bypass",          "",       "",      "",       ""),
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
