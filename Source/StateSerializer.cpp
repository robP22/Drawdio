#include "StateSerializer.h"
#include <cstring>

size_t StateSerializer::calculateSize(const SerializedState& state)
{
    // Header: 3 bytes (magic) + 1 byte (version) = 4 bytes
    // Grid data: TotalCells bytes
    // Pedal slots: PedalSlotCount bytes
    // Routing: variable, up to PedalSlotCount
    // Flag: 1 byte
    return 4 + TotalCells + PedalSlotCount + PedalSlotCount + 1;
}

StateSerializer::SerializedState StateSerializer::createState(
    const std::array<uint8_t, TotalCells>& gridData,
    const std::array<DspModuleType, PedalSlotCount>& pedalSlots,
    const std::vector<uint8_t>& manualRouting)
{
    SerializedState state;
    state.gridData = gridData;
    for (int i = 0; i < PedalSlotCount; ++i)
        state.pedalSlots[i] = pedalSlots[i];
    state.manualRouting = manualRouting;
    return state;
}

void StateSerializer::serialize(const SerializedState& state, std::vector<uint8_t>& outBlob)
{
    const size_t totalSize = calculateSize(state);
    outBlob.resize(totalSize, 0);

    // Header
    outBlob[0] = MagicBytes[0];
    outBlob[1] = MagicBytes[1];
    outBlob[2] = MagicBytes[2];
    outBlob[3] = Version;

    // Grid data
    std::memcpy(outBlob.data() + 4, state.gridData.data(), TotalCells);

    // Pedal slots
    const int layoutOffset = 4 + TotalCells;
    for (int i = 0; i < PedalSlotCount; ++i)
        outBlob[layoutOffset + i] = static_cast<uint8_t>(state.pedalSlots[i]);

    // Manual routing
    const int routingOffset = layoutOffset + PedalSlotCount;
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        if (i < static_cast<int>(state.manualRouting.size()))
            outBlob[routingOffset + i] = state.manualRouting[static_cast<size_t>(i)];
        else
            outBlob[routingOffset + i] = 0xFF;  // No routing
    }

    // Flag (reserved for future use)
    const int flagOffset = routingOffset + PedalSlotCount;
    outBlob[flagOffset] = 0;
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

    const size_t expectedSize = 4 + TotalCells + PedalSlotCount + PedalSlotCount + 1;
    if (sizeInBytes < expectedSize)
        return false;

    // Grid data
    std::memcpy(outState.gridData.data(), data + 4, TotalCells);

    // Validate and clamp grid values
    for (auto& val : outState.gridData)
        if (val > 4)
            val = 0;

    // Pedal slots
    const int layoutOffset = 4 + TotalCells;
    constexpr auto maxPedalType = static_cast<uint8_t>(DspModuleType::GRANULAR_DELAY);
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        uint8_t raw = data[layoutOffset + i];
        if (raw > maxPedalType)
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
            outState.manualRouting.push_back(slot);
    }

    return true;
}