#pragma once
#include <JuceHeader.h>
#include "PedalStructures.h"

class DrawdioProcessor;

class PedalComponent : public juce::Component
{
public:
    PedalComponent(DrawdioProcessor& processor, int slotIndex, DspModuleType initialType);
    ~PedalComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    void setPedalType(DspModuleType type);
    void setKnobValue(int knobIdx, float value);
    void syncFromProcessor();

    juce::Point<float> getInputJackPos() const;
    juce::Point<float> getOutputJackPos() const;

    static const char* typeName(DspModuleType t);
    static juce::String knobLabel(DspModuleType t, int idx);

private:
    void showTypePopup();
    void initKnob(juce::Slider& knob);

    DrawdioProcessor& audioProcessor;
    int m_slotIndex;
    DspModuleType m_currentType;

    juce::Slider m_knobs[4];
};
