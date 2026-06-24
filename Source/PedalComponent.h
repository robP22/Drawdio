#pragma once
#include <JuceHeader.h>
#include <array>

#include "PedalDefinition.h"
#include "PedalStructures.h"
#include "ResourceManager.h"
#include "IThemeProvider.h"
#include "SpriteKnob.h"

class DrawdioProcessor;

class PedalComponent : public juce::Component
{
public:
    PedalComponent(DrawdioProcessor& processor,
                   int slotIndex,
                   DspModuleType initialType,
                   const ResourceManager& resources,
                   const IThemeProvider& theme);
    ~PedalComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    void setKnobValue(int knobIdx, float value);
    void syncFromProcessor();
    juce::Point<float> getInputJackPos() const;
    juce::Point<float> getOutputJackPos() const;

private:
    void showTypePopup();
    void updateDefinition();
    void updateKnobBounds();
    juce::Rectangle<float> getLabelArea() const;

    static constexpr int kKnobCount = 4;

    DrawdioProcessor& audioProcessor;
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
    int m_slotIndex;
    DspModuleType m_currentType;
    const PedalDefinition* m_definition = nullptr;
    std::array<juce::Rectangle<float>, kKnobCount> m_knobBounds;
    std::array<std::unique_ptr<SpriteKnob>, kKnobCount> m_knobs;
    float m_knobDragStartValues[kKnobCount]{};
};
