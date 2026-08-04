#pragma once
#include <JuceHeader.h>
#include "Core/Contracts/ProcessorInterfaces.h"

class MixerStrip : public juce::Component, private juce::Timer
{
public:
    MixerStrip(IMixerStripModel& model, int slotIdx);
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent&) override;
    void timerCallback() override;
    void setPedalName(const juce::String& n) { if (n != m_pedalName) { m_pedalName = n; repaint(); } }

private:
    IMixerStripModel& m_model;
    int m_slotIndex;
    float m_displayPeak = 0.0f;
    float m_displayGain = 1.0f;
    bool m_dragging = false;
    juce::Rectangle<float> m_meterBounds;
    juce::Rectangle<float> m_sliderBounds;
    juce::String m_pedalName;
};
