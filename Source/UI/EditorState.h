#pragma once

#include <array>
#include <cstdint>

#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "State/ProjectState.h"

struct PedalViewState
{
    DspModuleType type = DspModuleType::BYPASS;
    std::array<float, KnobsPerPedal> knobValues{};
    std::array<float, KnobsPerPedal> linkStrength{};
    std::array<bool, KnobsPerPedal> linked{};
    std::array<float, KnobsPerPedal> linkRangeMins{};
    std::array<float, KnobsPerPedal> linkRangeMaxs{};
    float gain = 1.0f;
    float peak = 0.0f;
};

struct EditorUiSnapshot
{
    std::array<PedalViewState, PedalSlotCount> pedals{};
    std::array<uint8_t, TotalCells> gridData{};
    std::array<uint8_t, PedalSlotCount> manualRouting{};
    std::array<uint8_t, PedalSlotCount> routingOrder{};
    std::array<float, TotalKnobs> knobValues{};
    uint32_t overrideMask = 0;
    uint8_t manualRoutingSize = 0;
    uint8_t routingSize = 0;
    uint8_t barCount = 1;
    uint8_t sectionStartBar = 0;
    bool manualMode = false;
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    float inputMeter = 0.0f;
    float outputMeter = 0.0f;
    float playHeadBpm = 120.0f;
    double playHeadPpq = 0.0;
    bool playHeadPlaying = false;
    bool pendingConfig = false;
    uint32_t configurationRevision = 0;
    uint32_t sessionRevision = 0;
    uint32_t releaseQueueDrops = 0;
    EditorSessionState session;
};
