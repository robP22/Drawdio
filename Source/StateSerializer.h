#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "PedalStructures.h"

class StateSerializer
{
public:
    static constexpr uint8_t MagicBytes[3] = { 0x44, 0x52, 0x44 }; // 'DRD'
    static constexpr uint8_t Version = 0x03;

    // Serializable state structure
    struct SerializedState
    {
        std::array<uint8_t, TotalCells> gridData;
        std::array<DspModuleType, PedalSlotCount> pedalSlots;
        std::vector<uint8_t> manualRouting;
        std::array<float, PedalSlotCount * 4> knobValues{}; // 24 knob floats, zero-initialized
    };

    // Serialize processor state to memory block
    static void serialize(const SerializedState& state, std::vector<uint8_t>& outBlob);

    // Deserialize memory block to state
    static bool deserialize(const uint8_t* data, size_t sizeInBytes, SerializedState& outState);

    // Create SerializedState from current processor state
    static SerializedState createState(
        const std::array<uint8_t, TotalCells>& gridData,
        const std::array<DspModuleType, PedalSlotCount>& pedalSlots,
        const std::vector<uint8_t>& manualRouting,
        const std::array<float, PedalSlotCount * 4>& knobValues);

    // Validation helpers
    static bool isValidHeader(const uint8_t* data, size_t sizeInBytes);
    static size_t calculateSize(const SerializedState& state);
};