#include "StateSerializer.h"
#include <cstring>

size_t StateSerializer::calculateSize(const SerializedState& state)
{
    // Header: 3 bytes (magic) + 1 byte (version) = 4 bytes
    // Grid data: TotalCells bytes
    // Pedal slots: PedalSlotCount bytes
    // Routing: PedalSlotCount bytes
    // Flag: 1 byte
    // Knob values: PedalSlotCount * 4 * sizeof(float) bytes
    // Override mask: sizeof(uint32_t) bytes (v4+)
    return 4 + TotalCells + PedalSlotCount + PedalSlotCount + 1
         + static_cast<int>(PedalSlotCount * 4 * sizeof(float))
         + static_cast<int>(sizeof(uint32_t));
}

StateSerializer::SerializedState StateSerializer::createState(
    const std::array<uint8_t, TotalCells>& gridData,
    const std::array<DspModuleType, PedalSlotCount>& pedalSlots,
    const std::vector<uint8_t>& manualRouting,
    const std::array<float, PedalSlotCount * 4>& knobValues,
    uint32_t overrideMask)
{
    SerializedState state;
    state.gridData = gridData;
    for (int i = 0; i < PedalSlotCount; ++i)
        state.pedalSlots[i] = pedalSlots[i];
    state.manualRouting = manualRouting;
    state.knobValues = knobValues;
    state.overrideMask = overrideMask;
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

    // Flag — now version field for mask support
    const int flagOffset = routingOffset + PedalSlotCount;
    outBlob[flagOffset] = Version;

    // Knob values (24 floats = 96 bytes)
    const int knobOffset = flagOffset + 1;
    std::memcpy(outBlob.data() + knobOffset, state.knobValues.data(),
                PedalSlotCount * 4 * sizeof(float));

    // Override mask (4 bytes, v4+)
    const int maskOffset = knobOffset + static_cast<int>(PedalSlotCount * 4 * sizeof(float));
    std::memcpy(outBlob.data() + maskOffset, &state.overrideMask, sizeof(uint32_t));
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
    const size_t v3Size = v2Size + PedalSlotCount * 4 * sizeof(float);
    if (sizeInBytes < v2Size)
        return false;

    // Grid data
    std::memcpy(outState.gridData.data(), data + 4, TotalCells);

    // Validate and clamp grid values
    for (auto& val : outState.gridData)
        if (val > 10)
            val = 0;

    // Pedal slots
    const int layoutOffset = 4 + TotalCells;
    constexpr auto maxPedalType = static_cast<uint8_t>(DspModuleType::AUTOMATION_GENERATOR);
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
        {
            // Valid slot index (0-5). Values >= PedalSlotCount are 0xFF (no routing) - skip.
            outState.manualRouting.push_back(slot);
        }
        // If slot >= PedalSlotCount, it's 0xFF (no routing) - skip
    }

    // Knob values (v3+ only, backward compatible with v2)
    const int flagOffset = routingOffset + PedalSlotCount;
    const size_t knobOffset = static_cast<size_t>(flagOffset) + 1;
    if (sizeInBytes >= knobOffset + PedalSlotCount * 4 * sizeof(float))
    {
        std::memcpy(outState.knobValues.data(), data + knobOffset,
                    PedalSlotCount * 4 * sizeof(float));
    }
    else
    {
        // v2 save — leave knobValues at zero-initialized defaults
        outState.knobValues.fill(0.5f);
    }

    // Override mask (v4+ only, backward compatible with v2/v3)
    const size_t maskOffset = knobOffset + PedalSlotCount * 4 * sizeof(float);
    if (sizeInBytes >= maskOffset + sizeof(uint32_t))
    {
        std::memcpy(&outState.overrideMask, data + maskOffset, sizeof(uint32_t));
    }
    else
    {
        // v2/v3 save — default to clear (compiler fills in, UI shows canvas-derived values)
        outState.overrideMask = 0x00000000;
    }

    return true;
}