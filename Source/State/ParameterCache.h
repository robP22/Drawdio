#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include "Core/DrawdioConstants.h"

class ParameterCache
{
public:
    struct Snapshot
    {
        std::array<float, 24> values{};
        uint32_t revision = 0;
    };

    ParameterCache();

    void update(int physicalSlot, int knobIdx, float newValue);
    void store(int physicalSlot, int knobIdx, float value);
    void applyOffset(int physicalSlot, int knobIdx, float dragStartValue, float newValue);
    void clearOffsets();
    float getKnobDisplayValue(int slot, int knob, float compiledValue) const;
    void invalidateSlot(int physicalSlot);
    bool isOverridden(int physicalSlot, int knobIdx) const;
    uint32_t getOverrideMask() const { return m_validMask.load(std::memory_order_acquire); }
    Snapshot getSnapshot() const;

    float readRaw(int idx) const { return m_cache[static_cast<size_t>(idx)].load(std::memory_order_relaxed); }
    uint32_t readValidMask() const { return m_validMask.load(std::memory_order_acquire); }
    float readOffset(int idx) const { return m_offsets[static_cast<size_t>(idx)]; }

private:
    static size_t index(int slot, int knob) { return static_cast<size_t>(slot * 4 + knob); }

    std::atomic<uint32_t> m_revision{0};
    std::array<std::atomic<float>, 24> m_cache;
    std::atomic<uint32_t> m_validMask{0};
    std::array<float, 24> m_offsets{};
};
