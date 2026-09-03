#include "PedalState.h"

#include <algorithm>
#include <cmath>

PedalState::PedalState()
{
    for (auto& g : m_pedalGains)
        g.store(1.0f, std::memory_order_relaxed);
    for (auto& row : m_knobLinkRangeMins)
        for (auto& v : row)
            v.store(0.0f, std::memory_order_relaxed);
    for (auto& row : m_knobLinkRangeMaxs)
        for (auto& v : row)
            v.store(1.0f, std::memory_order_relaxed);
}

float PedalState::getPedalPeak(int slot) const
{
    if (slot >= 0 && slot < PedalSlotCount)
        return m_pedalPeaks[static_cast<size_t>(slot)].load(std::memory_order_relaxed);
    return 0.0f;
}

void PedalState::resetPedalPeaks()
{
    for (auto& p : m_pedalPeaks)
        p.store(0.0f, std::memory_order_relaxed);
}

float PedalState::getPedalGain(int slot) const
{
    if (slot >= 0 && slot < PedalSlotCount)
        return m_pedalGains[static_cast<size_t>(slot)].load(std::memory_order_relaxed);
    return 1.0f;
}

void PedalState::setPedalGain(int slot, float gain)
{
    if (slot >= 0 && slot < PedalSlotCount)
    {
        const float safeGain = std::isfinite(gain)
                                   ? std::clamp(gain, PedalGainMin, PedalGainMax)
                                   : 1.0f;
        m_pedalGains[static_cast<size_t>(slot)].store(safeGain, std::memory_order_relaxed);
    }
}

void PedalState::setKnobLink(int slot, int knob, bool linked, float strength)
{
    if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < KnobsPerPedal)
    {
        m_knobLinks[static_cast<size_t>(slot)][static_cast<size_t>(knob)].store(linked, std::memory_order_release);
        m_knobLinkStrengths[static_cast<size_t>(slot)][static_cast<size_t>(knob)].store(linked ? strength : 0.0f, std::memory_order_release);
    }
}

bool PedalState::isKnobLinked(int slot, int knob) const
{
    if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < KnobsPerPedal)
        return m_knobLinks[static_cast<size_t>(slot)][static_cast<size_t>(knob)].load(std::memory_order_acquire);
    return false;
}

float PedalState::getKnobLinkStrength(int slot, int knob) const
{
    if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < KnobsPerPedal)
        return m_knobLinkStrengths[static_cast<size_t>(slot)][static_cast<size_t>(knob)].load(std::memory_order_acquire);
    return 0.0f;
}

void PedalState::setKnobLinkRange(int slot, int knob, float rangeMin, float rangeMax)
{
    if (slot < 0 || slot >= PedalSlotCount || knob < 0 || knob >= KnobsPerPedal)
        return;
    const float lo = std::clamp(std::isfinite(rangeMin) ? rangeMin : 0.0f, 0.0f, 1.0f);
    const float hi = std::clamp(std::isfinite(rangeMax) ? rangeMax : 1.0f, 0.0f, 1.0f);
    const float mn = std::min(lo, hi - 0.05f);
    const float mx = std::max(hi, mn + 0.05f);
    const float clampedMin = std::clamp(mn, 0.0f, 0.95f);
    const float clampedMax = std::clamp(mx, 0.05f, 1.0f);
    m_knobLinkRangeMins[static_cast<size_t>(slot)][static_cast<size_t>(knob)].store(clampedMin, std::memory_order_release);
    m_knobLinkRangeMaxs[static_cast<size_t>(slot)][static_cast<size_t>(knob)].store(clampedMax, std::memory_order_release);
}

float PedalState::getKnobLinkRangeMin(int slot, int knob) const
{
    if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < KnobsPerPedal)
        return m_knobLinkRangeMins[static_cast<size_t>(slot)][static_cast<size_t>(knob)].load(std::memory_order_acquire);
    return 0.0f;
}

float PedalState::getKnobLinkRangeMax(int slot, int knob) const
{
    if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < KnobsPerPedal)
        return m_knobLinkRangeMaxs[static_cast<size_t>(slot)][static_cast<size_t>(knob)].load(std::memory_order_acquire);
    return 1.0f;
}
