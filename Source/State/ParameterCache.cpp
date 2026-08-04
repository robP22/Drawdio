#include "ParameterCache.h"
#include <cmath>

ParameterCache::ParameterCache()
{
    for (auto& p : m_cache)
        p.store(0.5f, std::memory_order_relaxed);
}

void ParameterCache::update(int physicalSlot, int knobIdx, float newValue)
{
    if (!std::isfinite(newValue))
        newValue = 0.0f;

    if (physicalSlot >= 0 && physicalSlot < PedalSlotCount && knobIdx >= 0 && knobIdx < 4)
    {
        size_t idx = index(physicalSlot, knobIdx);
        m_cache[idx].store(newValue, std::memory_order_release);
        uint32_t mask = m_validMask.load(std::memory_order_relaxed);
        mask |= (1u << idx);
        m_validMask.store(mask, std::memory_order_release);
        m_revision.fetch_add(1, std::memory_order_acq_rel);
    }
}

void ParameterCache::store(int physicalSlot, int knobIdx, float value)
{
    if (physicalSlot >= 0 && physicalSlot < PedalSlotCount && knobIdx >= 0 && knobIdx < 4)
    {
        size_t idx = index(physicalSlot, knobIdx);
        m_cache[idx].store(value, std::memory_order_release);
        m_revision.fetch_add(1, std::memory_order_acq_rel);
    }
}

void ParameterCache::applyOffset(int physicalSlot, int knobIdx, float dragStartValue, float newValue)
{
    if (physicalSlot >= 0 && physicalSlot < PedalSlotCount && knobIdx >= 0 && knobIdx < 4)
    {
        size_t idx = index(physicalSlot, knobIdx);
        m_offsets[idx] += newValue - dragStartValue;
        m_cache[idx].store(newValue, std::memory_order_release);
        uint32_t mask = m_validMask.load(std::memory_order_relaxed);
        mask |= (1u << idx);
        m_validMask.store(mask, std::memory_order_release);
        m_revision.fetch_add(1, std::memory_order_acq_rel);
    }
}

void ParameterCache::clearOffsets()
{
    m_offsets.fill(0.0f);
    m_validMask.store(0, std::memory_order_release);
}

float ParameterCache::getKnobDisplayValue(int slot, int knob, float compiledValue) const
{
    if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < 4)
    {
        size_t idx = index(slot, knob);
        float val = compiledValue + m_offsets[idx];
        return val < 0.0f ? 0.0f : (val > 1.0f ? 1.0f : val);
    }
    return compiledValue;
}

void ParameterCache::invalidateSlot(int physicalSlot)
{
    if (physicalSlot < 0 || physicalSlot >= PedalSlotCount) return;
    uint32_t mask = m_validMask.load(std::memory_order_relaxed);
    for (int k = 0; k < 4; ++k)
        mask &= ~(1u << index(physicalSlot, k));
    m_validMask.store(mask, std::memory_order_release);
}

bool ParameterCache::isOverridden(int physicalSlot, int knobIdx) const
{
    if (physicalSlot < 0 || physicalSlot >= PedalSlotCount || knobIdx < 0 || knobIdx >= 4)
        return false;
    size_t idx = index(physicalSlot, knobIdx);
    uint32_t mask = m_validMask.load(std::memory_order_acquire);
    return (mask & (1u << idx)) != 0;
}

ParameterCache::Snapshot ParameterCache::getSnapshot() const
{
    Snapshot snap;
    snap.revision = m_revision.load(std::memory_order_acquire);
    for (size_t i = 0; i < snap.values.size(); ++i)
        snap.values[i] = m_cache[i].load(std::memory_order_relaxed);
    return snap;
}
