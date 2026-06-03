#include "DspState.h"
#include <algorithm>

DspState::DspState()
{
    for (auto& param : m_parameters)
        param.store(0.5f, std::memory_order_relaxed);

    for (auto& slot : m_pedalSlots)
        slot = DspModuleType::BYPASS;
}

void DspState::setParameterValue(int slot, int paramIdx, float value)
{
    if (slot < 0 || slot >= MaxPedalSlots || paramIdx < 0 || paramIdx >= 4)
        return;

    const int idx = slot * 4 + paramIdx;
    m_parameters[static_cast<size_t>(idx)].store(value, std::memory_order_release);
}

float DspState::getParameterValue(int slot, int paramIdx) const
{
    if (slot < 0 || slot >= MaxPedalSlots || paramIdx < 0 || paramIdx >= 4)
        return 0.5f;

    const int idx = slot * 4 + paramIdx;
    return m_parameters[static_cast<size_t>(idx)].load(std::memory_order_acquire);
}

bool DspState::tryMarkDirty()
{
    bool expected = false;
    return m_dirty.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
}

bool DspState::isDirty() const
{
    return m_dirty.load(std::memory_order_acquire);
}

void DspState::clearDirty()
{
    m_dirty.store(false, std::memory_order_release);
}

bool DspState::consumeNeedsRepaint()
{
    bool expected = true;
    return m_needsRepaint.compare_exchange_strong(expected, false, std::memory_order_acq_rel, std::memory_order_acquire);
}

void DspState::requestRepaint()
{
    m_needsRepaint.store(true, std::memory_order_release);
}

uint32_t DspState::getRevision() const
{
    return m_revision.load(std::memory_order_acquire);
}

void DspState::incrementRevision()
{
    m_revision.fetch_add(1, std::memory_order_acq_rel);
}

void DspState::setPedalType(int slot, DspModuleType type)
{
    if (slot < 0 || slot >= MaxPedalSlots)
        return;
    m_pedalSlots[static_cast<size_t>(slot)] = type;
}

DspModuleType DspState::getPedalType(int slot) const
{
    if (slot < 0 || slot >= MaxPedalSlots)
        return DspModuleType::BYPASS;
    return m_pedalSlots[static_cast<size_t>(slot)];
}

void DspState::setManualRouting(const std::vector<uint8_t>& routing)
{
    std::lock_guard<std::mutex> lock(m_routingMutex);
    m_manualRouting = routing;
}

std::vector<uint8_t> DspState::getManualRouting() const
{
    std::lock_guard<std::mutex> lock(m_routingMutex);
    return m_manualRouting;
}

DspState::ParameterSnapshot DspState::getSnapshot() const
{
    ParameterSnapshot snap;
    snap.revision = m_revision.load(std::memory_order_acquire);

    for (int i = 0; i < MaxParameters; ++i)
        snap.values[static_cast<size_t>(i)] = m_parameters[static_cast<size_t>(i)].load(std::memory_order_acquire);

    for (int i = 0; i < MaxPedalSlots; ++i)
        snap.pedalTypes[static_cast<size_t>(i)] = m_pedalSlots[static_cast<size_t>(i)];

    {
        std::lock_guard<std::mutex> lock(m_routingMutex);
        snap.routing = m_manualRouting;
    }

    return snap;
}

void DspState::setGridData(const std::array<uint8_t, TotalCells>& data)
{
    m_gridData = data;
}

std::array<uint8_t, TotalCells> DspState::getGridData() const
{
    return m_gridData;
}