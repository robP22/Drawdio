#pragma once
#include <atomic>
#include "State/AutomationEnvelope.h"

class AutomationPlayer
{
public:
    void setEnvelope(const AutomationEnvelope& env) { m_envelope = env; }
    void setBarCount(int bars) { m_barCount = bars; }
    int getBarCount() const { return m_barCount; }
    void setSectionStartBar(int s) { m_sectionStartBar = s; }
    int getSectionStartBar() const { return m_sectionStartBar; }

    void tick(double ppqPosition, float bpm, bool isPlaying);
    float getValue() const { return m_currentValue.load(std::memory_order_relaxed); }
    float getPlayheadTime() const { return m_playheadTime; }

private:
    AutomationEnvelope m_envelope;
    std::atomic<float> m_currentValue{0.5f};
    float m_playheadTime = 0.0f;
    int m_barCount = 1;
    int m_sectionStartBar = 0;
};
