#pragma once

#include <JuceHeader.h>
#include <array>

#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"


struct NormalizedControlBounds
{
    float centreX = 0.5f;
    float centreY = 0.5f;
    float width = 0.25f;
    float height = 0.20f;
};

struct PedalParameterDefinition
{
    ParameterDescriptor param;
    const char* label = "Param";
    // Evenly spaced detent count on [0, 1]; 0 disables snapping.
    int snapSteps = 0;

    constexpr PedalParameterDefinition(uint16_t token, const char* lbl, float mn, float mx, float dv, int snaps = 0)
        : param{token, mn, mx, dv, dv, false, 0}, label(lbl), snapSteps(snaps)
    {
    }};

std::array<NormalizedControlBounds, 5> knobLayoutForCount(int count);

struct PedalDefinition
{
    DspModuleType type = DspModuleType::BYPASS;
    const char* displayName = "Unknown";
    int knobCount = 0;
    std::array<PedalParameterDefinition, 4> parameters;
};

namespace PedalDefinitions
{
    const PedalDefinition& get(DspModuleType type);
    const PedalDefinition& fallback();
    juce::String getDisplayName(DspModuleType type);
    int snapSteps(DspModuleType type, int knobIdx);
    float snapValue(DspModuleType type, int knobIdx, float value);
}
