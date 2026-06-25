#pragma once
#include <JuceHeader.h>
#include "AutomationEnvelope.h"

class AutomationDisplay : public juce::Component, private juce::Timer
{
public:
    AutomationDisplay();

    void setEnvelope(const AutomationEnvelope& env) { m_envelope = env; m_needsRepaint = true; }
    void setPlayheadTime(float t) { if (t != m_playheadTime) { m_playheadTime = t; repaint(); } }
    void setBarCount(int bars)
    {
        m_activeBars = bars;
        m_sectionStartBar = std::min(m_sectionStartBar, 8 - bars);
        m_needsRepaint = true;
    }
    int getBarCount() const { return m_activeBars; }

    void setSectionStart(int bar)
    {
        m_sectionStartBar = std::max(0, std::min(bar, 8 - m_activeBars));
        m_needsRepaint = true;
    }
    int getSectionStart() const { return m_sectionStartBar; }

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    void timerCallback() override { if (m_needsRepaint) { repaint(); m_needsRepaint = false; } }

    std::function<void(int)> onBarCountChanged;
    std::function<void(int)> onSectionChanged;

private:
    AutomationEnvelope m_envelope;
    float m_playheadTime = 0.0f;
    int m_activeBars = 1;
    int m_sectionStartBar = 0;
    bool m_needsRepaint = false;
};
