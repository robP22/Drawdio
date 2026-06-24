#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>
#include "PedalStructures.h"

class StateSerializer
{
public:
    static constexpr uint8_t MagicBytes[3] = { 0x44, 0x52, 0x44 }; // 'DRD'
    static constexpr uint8_t Version = 0x05;

    // Serializable state structure
    struct SerializedState
    {
        std::array<uint8_t, TotalCells> gridData;
        std::array<DspModuleType, PedalSlotCount> pedalSlots;
        std::vector<uint8_t> manualRouting;
        std::array<float, PedalSlotCount * 4> knobValues{}; // 24 knob floats, zero-initialized
        uint32_t overrideMask = 0xFFFFFFFF; // bit n = (slot*4+knob) → 1 = manually overridden
        uint8_t barCount = 1;
        uint8_t sectionStartBar = 0;
        uint8_t manualMode = 0;
    };

    // Serialize processor state to memory block
    static void serialize(const SerializedState& state, juce::MemoryBlock& outBlob);

    // Deserialize memory block to state
    static bool deserialize(const uint8_t* data, size_t sizeInBytes, SerializedState& outState);

    // Create SerializedState from current processor state
    static SerializedState createState(
        const std::array<uint8_t, TotalCells>& gridData,
        const std::array<DspModuleType, PedalSlotCount>& pedalSlots,
        const std::vector<uint8_t>& manualRouting,
        const std::array<float, PedalSlotCount * 4>& knobValues,
        uint32_t overrideMask,
        uint8_t barCount, uint8_t sectionStart, uint8_t manualMode);

    // Validation helpers
    static bool isValidHeader(const uint8_t* data, size_t sizeInBytes);
    static size_t calculateSize(const SerializedState& state);
};