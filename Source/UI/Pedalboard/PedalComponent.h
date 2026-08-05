#pragma once
#include <JuceHeader.h>
#include <array>

#include "Core/DspModuleType.h"
#include "State/PedalDefinition.h"
#include "Core/Contracts/IResourceProvider.h"
#include "UI/Theme/IThemeProvider.h"
#include "UI/Controls/SpriteKnob.h"
#include "Core/Contracts/ProcessorInterfaces.h"
#include "Core/Contracts/IComponentBounds.h"

class PedalComponent : public juce::Component, public IComponentBounds
{
public:
    PedalComponent(IPedalComponentModel& model,
                   int slotIndex,
                   DspModuleType initialType,
                   const IResourceProvider& resources,
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
    juce::Rectangle<int> getBounds() const override;
    void setBounds(juce::Rectangle<int>) override;
    juce::Point<float> getInputJackPos() const override;
    juce::Point<float> getOutputJackPos() const override;

private:
    void showTypePopup();
    void updateKnobBounds();
    void onKnobDragStart(int knobIdx, float value);
    void onKnobValueChanged(int knobIdx, float value);
    void onKnobRightClick(int knobIdx);
    juce::Rectangle<float> getLabelArea() const;

    static constexpr int kKnobCount = 4;

    IPedalComponentModel& m_model;
    const IResourceProvider& m_resources;
    const IThemeProvider& m_theme;
    int m_slotIndex;
    DspModuleType m_currentType;
    const PedalDefinition* m_definition = nullptr;
    std::array<juce::Rectangle<float>, kKnobCount> m_knobBounds;
    std::array<std::unique_ptr<SpriteKnob>, kKnobCount> m_knobs;
    float m_knobDragStartValues[kKnobCount]{};
    juce::Font m_lcdFont;
    juce::Font m_labelFont;
};
