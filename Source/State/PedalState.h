#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include "Core/DrawdioConstants.h"

class PedalState
{
public:
    PedalState();

    float getPedalPeak(int slot) const;
    void resetPedalPeaks();

    float getPedalGain(int slot) const;
    void setPedalGain(int slot, float gain);

    float getInputGain() const { return m_inputGain.load(std::memory_order_relaxed); }
    void setInputGain(float g) { m_inputGain.store(g, std::memory_order_relaxed); }
    float getOutputGain() const { return m_outputGain.load(std::memory_order_relaxed); }
    void setOutputGain(float g) { m_outputGain.store(g, std::memory_order_relaxed); }

    void setKnobLink(int slot, int knob, bool linked, float strength = 1.0f);
    bool isKnobLinked(int slot, int knob) const;
    float getKnobLinkStrength(int slot, int knob) const;

    // Raw atomic access for audio thread (processChainBlock)
    std::atomic<float>& peakRef(int slot) { return m_pedalPeaks[static_cast<size_t>(slot)]; }
    std::atomic<float>& gainRef(int slot) { return m_pedalGains[static_cast<size_t>(slot)]; }
    bool knobLinked(int slot, int knob) const { return m_knobLinks[static_cast<size_t>(slot)][static_cast<size_t>(knob)].load(std::memory_order_acquire); }
    float knobLinkStrength(int slot, int knob) const { return m_knobLinkStrengths[static_cast<size_t>(slot)][static_cast<size_t>(knob)].load(std::memory_order_acquire); }

private:
    std::array<std::atomic<float>, PedalSlotCount> m_pedalPeaks;
    std::array<std::atomic<float>, PedalSlotCount> m_pedalGains;
    std::atomic<float> m_inputGain{1.0f};
    std::atomic<float> m_outputGain{1.0f};
    std::array<std::array<std::atomic<bool>, 4>, PedalSlotCount> m_knobLinks{};
    std::array<std::array<std::atomic<float>, 4>, PedalSlotCount> m_knobLinkStrengths{};
};
