#pragma once
#include <JuceHeader.h>
#include "PedalDefinition.h"
#include "PedalStructures.h"
#include "ResourceManager.h"
#include "ThemeManager.h"

class DrawdioProcessor;

class PedalComponent : public juce::Component
{
public:
    PedalComponent(DrawdioProcessor& processor,
                   int slotIndex,
                   DspModuleType initialType,
                   const ResourceManager& resources,
                   const ThemeManager& theme);
    ~PedalComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    void setKnobValue(int knobIdx, float value);
    void syncFromProcessor();

    juce::Point<float> getInputJackPos() const;
    juce::Point<float> getOutputJackPos() const;

    static const char* typeName(DspModuleType t);
    static juce::String knobLabel(DspModuleType t, int idx);

private:
    void showTypePopup();
    void initKnob(juce::Slider& knob);
    juce::Rectangle<float> getBodyBounds() const;
    void updateDefinition();

    DrawdioProcessor& audioProcessor;
    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
    int m_slotIndex;
    DspModuleType m_currentType;
    const PedalDefinition* m_definition = nullptr;

    juce::Slider m_knobs[4];
};
