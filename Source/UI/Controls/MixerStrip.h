#pragma once
#include <JuceHeader.h>
#include <functional>
#include "Core/DspModuleType.h"

struct MixerStripViewState
{
    DspModuleType type = DspModuleType::BYPASS;
    float peak = 0.0f;
    float gain = 1.0f;
};

class MixerStrip : public juce::Component
{
public:
    struct Actions
    {
        std::function<void(int, float)> setGain;
    };

    MixerStrip(int slotIdx, MixerStripViewState state, Actions actions);
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent&) override;
    void tick();
    void setViewState(const MixerStripViewState& state);
    void setPeak(float peak);
    void setPedalName(const juce::String& n) { if (n != m_pedalName) { m_pedalName = n; repaint(); } }

private:
    int m_slotIndex;
    Actions m_actions;
    DspModuleType m_type = DspModuleType::BYPASS;
    float m_targetPeak = 0.0f;
    float m_displayPeak = 0.0f;
    float m_displayGain = 1.0f;
    bool m_dragging = false;
    juce::Rectangle<float> m_meterBounds;
    juce::Rectangle<float> m_sliderBounds;
    juce::String m_pedalName;
};
