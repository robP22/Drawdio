#pragma once

#include <array>
#include <atomic>

#include "Core/DrawdioConstants.h"

class CompiledParameterBank
{
public:
    CompiledParameterBank()
    {
        for (auto& value : m_values)
            value.store(0.5f, std::memory_order_relaxed);
    }

    void store(int slot, int knob, float value)
    {
        if (slot < 0 || slot >= PedalSlotCount || knob < 0 || knob >= KnobsPerPedal)
            return;
        m_values[static_cast<size_t>(slot * KnobsPerPedal + knob)].store(value, std::memory_order_release);
    }

    float load(int slot, int knob) const
    {
        if (slot < 0 || slot >= PedalSlotCount || knob < 0 || knob >= KnobsPerPedal)
            return 0.5f;
        return m_values[static_cast<size_t>(slot * KnobsPerPedal + knob)].load(std::memory_order_acquire);
    }

private:
    std::array<std::atomic<float>, TotalKnobs> m_values;
};
