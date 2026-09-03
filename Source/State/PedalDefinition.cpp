#include "PedalDefinition.h"
#include <algorithm>
#include <cmath>

std::array<NormalizedControlBounds, 5> knobLayoutForCount(int count)
{
    static const std::array<NormalizedControlBounds, 5> layouts[5] = {
        {{
            { 0.50f, 0.455f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
        }},
        {{
            { 0.50f, 0.345f, 0.34f, 0.22f },
            { 0.50f, 0.565f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
        }},
        {{
            { 0.31f, 0.345f, 0.34f, 0.22f },
            { 0.69f, 0.345f, 0.34f, 0.22f },
            { 0.50f, 0.565f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
        }},
        {{
            { 0.31f, 0.345f, 0.34f, 0.22f },
            { 0.69f, 0.345f, 0.34f, 0.22f },
            { 0.31f, 0.565f, 0.34f, 0.22f },
            { 0.69f, 0.565f, 0.34f, 0.22f },
            { 0.50f, 0.455f, 0.34f, 0.22f },
        }},
        {{
            { 0.31f, 0.345f, 0.34f, 0.22f },
            { 0.69f, 0.345f, 0.34f, 0.22f },
            { 0.31f, 0.565f, 0.34f, 0.22f },
            { 0.69f, 0.565f, 0.34f, 0.22f },
            { 0.50f, 0.685f, 0.34f, 0.22f },
        }},
    };

    const int idx = std::max(1, std::min(5, count));
    return layouts[idx - 1];
}

namespace
{
PedalDefinition makeDefinition(DspModuleType type, const char* displayName,
                                const char* l0, const char* l1,
                                const char* l2, const char* l3,
                                int s0 = 0, int s1 = 0, int s2 = 0, int s3 = 0,
                                float dv0 = 0.5f, float dv1 = 0.5f,
                                float dv2 = 0.5f, float dv3 = 0.5f)
{
    const char* labels[4] = { l0, l1, l2, l3 };
    int knobCount = 0;
    for (int i = 0; i < 4; ++i)
        if (labels[i] != nullptr && labels[i][0] != '\0')
            ++knobCount;

    return {
        type,
        displayName,
        knobCount,
        {{
            PedalParameterDefinition(ParamToken::Knob0, l0, 0.0f, 1.0f, dv0, s0),
            PedalParameterDefinition(ParamToken::Knob1, l1, 0.0f, 1.0f, dv1, s1),
            PedalParameterDefinition(ParamToken::Knob2, l2, 0.0f, 1.0f, dv2, s2),
            PedalParameterDefinition(ParamToken::Knob3, l3, 0.0f, 1.0f, dv3, s3)
        }}
    };
}

const std::array<PedalDefinition, 27>& definitions()
{
    static_assert(27 == static_cast<size_t>(DspModuleType::RESERVED_REMOVED_OCTAVER) + 1,
                  "PedalDefinitions array must match DspModuleType enum count");
    static const std::array<PedalDefinition, 27> defs {{
makeDefinition(DspModuleType::BYPASS, "Bypass",             "Mix",    "",       "",       ""),
        makeDefinition(DspModuleType::WAVESHAPER,         "Waveshaper",      "Mix",    "",       "Drive",  ""),
        makeDefinition(DspModuleType::CHORUS,             "Chorus",          "Mix",    "Depth", "",       "Rate"),
        makeDefinition(DspModuleType::MULTI_MODE_FILTER,  "Multi-Mode Filter", "Mode", "Mix",  "Cutoff", ""),
        makeDefinition(DspModuleType::PITCH_SHIFTER,      "Pitch Shifter",   "Mix",    "",       "Pitch",  "", 0, 0, 25, 0),
        makeDefinition(DspModuleType::VCA_COMPRESSOR,     "VCA Compressor",  "Attack", "Mix",    "Thresh", "Level"),
        makeDefinition(DspModuleType::GLITCH_STUTTER,     "Glitch Stutter",  "Intens", "Mix",    "Random", "Smooth"),
        makeDefinition(DspModuleType::DIFFUSED_REVERB,    "Diffused Reverb", "Mix",    "Size",  "",       "Decay"),
        makeDefinition(DspModuleType::WAVEFOLDER,         "Wavefolder",      "Mix",    "Fold",  "",       ""),
        makeDefinition(DspModuleType::FORMANT_SHIFTER,    "Formant Shifter", "Mix",    "Shift", "Formant","Q"),
        makeDefinition(DspModuleType::RETIME,             "ReTime",           "Mix",    "Time",  "Bars",   "Shift", 0, 5, 4, 0),
        makeDefinition(DspModuleType::DELAY,              "Delay",            "Mix",    "Time",  "Feed",   "Damp"),
        makeDefinition(DspModuleType::PLATE_REVERB,       "Plate Reverb",     "Mix",    "Size",  "Decay",  ""),
        makeDefinition(DspModuleType::SIDECHAIN,          "Sidechain",        "Rate",   "Shape", "Depth",  "Mix", 5, 0, 0, 0),
        makeDefinition(DspModuleType::GRANULAR_DELAY,     "Granular Delay",   "Mix",    "Spread","Size",   "Delay"),
        makeDefinition(DspModuleType::COMB_RESONATOR,     "Comb Resonator",   "Freq",   "Mix",   "",       ""),
        makeDefinition(DspModuleType::SPECTRAL_FREEZE,    "Spectral Freeze",  "Freeze", "Mix",    "Offset", ""),
        makeDefinition(DspModuleType::FREQ_SHIFTER,       "Frequency Shifter","Shift",  "Mix",   "",       ""),
        makeDefinition(DspModuleType::REVERSE,            "Reverse",          "Mix",    "",       "Smooth", "Density"),
        makeDefinition(DspModuleType::GRAIN_SCRUBBER,     "Grain Scrubber",   "Position","Mix",  "",       "Rate"),
        makeDefinition(DspModuleType::SPECTRAL_FILTER,    "Spectral Filter",  "Width",  "Center","Q",      "Mix"),
        makeDefinition(DspModuleType::CONVOLUTION_REVERB, "Convolution Reverb","Mix",   "Size",  "Width",  "Damp", 0, 0, 0, 15),
        makeDefinition(DspModuleType::HP_LP_FILTER,       "HP/LP Filter",     "Mix",    "High",  "Low",    "Reso", 0, 0, 0, 0, 0.5f, 0.0f, 1.0f, 0.0f),
        makeDefinition(DspModuleType::BITCRUSHER,         "Bitcrusher",       "Mix",    "Rate",  "Bits",   "Filter", 0, 0, 15, 0),
        makeDefinition(DspModuleType::TREMOLO,            "Tremolo",          "Mix",    "Rate",  "Depth",  "Shape", 0, 0, 0, 3),
        makeDefinition(DspModuleType::FLANGER,            "Flanger",          "Mix",    "Rate",  "Depth",  "Feed"),
        makeDefinition(DspModuleType::RESERVED_REMOVED_OCTAVER, "Bypass",     "",       "",      "",       ""),
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

int snapSteps(DspModuleType type, int knobIdx)
{
    const auto& def = get(type);
    if (knobIdx >= 0 && knobIdx < 4)
        return def.parameters[static_cast<size_t>(knobIdx)].snapSteps;
    return 0;
}

float snapValue(DspModuleType type, int knobIdx, float value)
{
    const int steps = snapSteps(type, knobIdx);
    if (steps < 2)
        return value;
    const float clamped = juce::jlimit(0.0f, 1.0f, value);
    return std::round(clamped * static_cast<float>(steps - 1)) / static_cast<float>(steps - 1);
}
}
