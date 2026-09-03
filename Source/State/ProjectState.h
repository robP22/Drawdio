#pragma once

#include <array>
#include <cstdint>

#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"

struct PresetState
{
    std::array<uint8_t, TotalCells> gridData{};
    std::array<DspModuleType, PedalSlotCount> pedalSlots{};
    std::array<uint8_t, PedalSlotCount> manualRouting{};
    uint8_t manualRoutingSize = 0;
    std::array<float, TotalKnobs> knobValues{};
    uint32_t overrideMask = 0;
    uint8_t barCount = 1;
    uint8_t sectionStartBar = 0;
    uint8_t manualMode = 0;
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    std::array<float, PedalSlotCount> pedalGains{};
    uint32_t linkFlags = 0;
    std::array<float, TotalKnobs> linkRangeMins{};
    std::array<float, TotalKnobs> linkRangeMaxs{};

    PresetState()
    {
        pedalSlots.fill(DspModuleType::BYPASS);
        knobValues.fill(0.5f);
        pedalGains.fill(1.0f);
        linkRangeMins.fill(0.0f);
        linkRangeMaxs.fill(1.0f);
    }
};

struct EditorSessionState
{
    uint8_t selectedColour = 3;
    uint8_t selectedTool = 0;
    int8_t selectedPedal = -1;
    uint8_t brushSizeIndex = 0;
};

struct ProjectState
{
    PresetState preset;
    EditorSessionState session;
};
