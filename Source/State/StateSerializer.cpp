#include "StateSerializer.h"
#include <cstring>

size_t StateSerializer::calculateSize(const SerializedState& state)
{
    // Header: 3 bytes (magic) + 1 byte (version) = 4 bytes
    // Grid data: TotalCells bytes
    // Pedal slots: PedalSlotCount bytes
    // Routing: PedalSlotCount bytes
    // Flag: 1 byte
    // Knob values: TotalKnobs * sizeof(float) bytes
    // Override mask: sizeof(uint32_t) bytes (v4+)
    return 4 + TotalCells + PedalSlotCount + PedalSlotCount + 1
         + static_cast<int>(TotalKnobs * sizeof(float))
         + static_cast<int>(sizeof(uint32_t))
         + 3;
}

StateSerializer::SerializedState StateSerializer::createState(
    const std::array<uint8_t, TotalCells>& gridData,
    const std::array<DspModuleType, PedalSlotCount>& pedalSlots,
    const std::vector<uint8_t>& manualRouting,
    const std::array<float, TotalKnobs>& knobValues,
    uint32_t overrideMask,
    uint8_t barCount, uint8_t sectionStart, uint8_t manualMode)
{
    SerializedState state;
    state.gridData = gridData;
    for (int i = 0; i < PedalSlotCount; ++i)
        state.pedalSlots[i] = pedalSlots[i];
    state.manualRouting = manualRouting;
    state.knobValues = knobValues;
    state.overrideMask = overrideMask;
    state.barCount = barCount;
    state.sectionStartBar = sectionStart;
    state.manualMode = manualMode;
    return state;
}

void StateSerializer::serialize(const SerializedState& state, juce::MemoryBlock& outBlob)
{
    const size_t totalSize = calculateSize(state);
    outBlob.setSize(totalSize, true);
    auto* data = static_cast<uint8_t*>(outBlob.getData());

    // Header
    data[0] = MagicBytes[0];
    data[1] = MagicBytes[1];
    data[2] = MagicBytes[2];
    data[3] = Version;

    // Grid data
    std::memcpy(data + 4, state.gridData.data(), TotalCells);

    // Pedal slots
    const int layoutOffset = 4 + TotalCells;
    for (int i = 0; i < PedalSlotCount; ++i)
        data[layoutOffset + i] = static_cast<uint8_t>(state.pedalSlots[i]);

    // Manual routing
    const int routingOffset = layoutOffset + PedalSlotCount;
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        if (i < static_cast<int>(state.manualRouting.size()))
            data[routingOffset + i] = state.manualRouting[static_cast<size_t>(i)];
        else
            data[routingOffset + i] = 0xFF;
    }

    // Flag
    const int flagOffset = routingOffset + PedalSlotCount;
    data[flagOffset] = Version;

    // Knob values (24 floats = 96 bytes)
    const int knobOffset = flagOffset + 1;
    std::memcpy(data + knobOffset, state.knobValues.data(),
                TotalKnobs * sizeof(float));

    // Override mask (4 bytes)
    const int maskOffset = knobOffset + static_cast<int>(TotalKnobs * sizeof(float));
    std::memcpy(data + maskOffset, &state.overrideMask, sizeof(uint32_t));

    // Flags (3 bytes): barCount, sectionStartBar, manualMode (v5+)
    const int flagsOffset = maskOffset + static_cast<int>(sizeof(uint32_t));
    data[flagsOffset] = state.barCount;
    data[flagsOffset + 1] = state.sectionStartBar;
    data[flagsOffset + 2] = state.manualMode;
}

bool StateSerializer::isValidHeader(const uint8_t* data, size_t sizeInBytes)
{
    if (sizeInBytes < 4)
        return false;

    return data[0] == MagicBytes[0] &&
           data[1] == MagicBytes[1] &&
           data[2] == MagicBytes[2];
}

bool StateSerializer::deserialize(const uint8_t* data, size_t sizeInBytes, SerializedState& outState)
{
    if (!isValidHeader(data, sizeInBytes))
        return false;

    const size_t v2Size = 4 + TotalCells + PedalSlotCount + PedalSlotCount + 1;
    const size_t v3Size = v2Size + TotalKnobs * sizeof(float);
    if (sizeInBytes < v2Size)
        return false;

    // Grid data
    std::memcpy(outState.gridData.data(), data + 4, TotalCells);

    // Validate and clamp grid values
    for (auto& val : outState.gridData)
        if (val > 12)
            val = 0;

    // Pedal slots
    const int layoutOffset = 4 + TotalCells;
    constexpr auto maxPedalType = static_cast<uint8_t>(DspModuleType::RESERVED_REMOVED_OCTAVER);
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        uint8_t raw = data[layoutOffset + i];
        if (raw > maxPedalType ||
            raw == static_cast<uint8_t>(DspModuleType::RESERVED_REMOVED_RANDOM_MODULATOR) ||
            raw == static_cast<uint8_t>(DspModuleType::RESERVED_REMOVED_OCTAVER))
            raw = 0;
        outState.pedalSlots[i] = static_cast<DspModuleType>(raw);
    }

    // Manual routing
    outState.manualRouting.clear();
    const int routingOffset = layoutOffset + PedalSlotCount;
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        uint8_t slot = data[routingOffset + i];
        if (slot < PedalSlotCount)
        {
            // Valid slot index (0-5). Values >= PedalSlotCount are 0xFF (no routing) - skip.
            outState.manualRouting.push_back(slot);
        }
        // If slot >= PedalSlotCount, it's 0xFF (no routing) - skip
    }

    // Knob values (v3+ only, backward compatible with v2)
    const int flagOffset = routingOffset + PedalSlotCount;
    const size_t knobOffset = static_cast<size_t>(flagOffset) + 1;
    if (sizeInBytes >= knobOffset + TotalKnobs * sizeof(float))
    {
        std::memcpy(outState.knobValues.data(), data + knobOffset,
                    TotalKnobs * sizeof(float));
    }
    else
    {
        // v2 save — leave knobValues at zero-initialized defaults
        outState.knobValues.fill(0.5f);
    }

    // Override mask (v4+ only, backward compatible with v2/v3)
    const size_t maskOffset = knobOffset + TotalKnobs * sizeof(float);
    if (sizeInBytes >= maskOffset + sizeof(uint32_t))
    {
        std::memcpy(&outState.overrideMask, data + maskOffset, sizeof(uint32_t));
    }
    else
    {
        // v2/v3 save — default to clear (compiler fills in, UI shows canvas-derived values)
        outState.overrideMask = 0x00000000;
    }

    // Flags (v5+ only): barCount, sectionStartBar, manualMode
    const size_t flagsOffset = maskOffset + sizeof(uint32_t);
    if (sizeInBytes >= flagsOffset + 3)
    {
        outState.barCount = data[flagsOffset];
        outState.sectionStartBar = data[flagsOffset + 1];
        outState.manualMode = data[flagsOffset + 2];
    }

    return true;
}
