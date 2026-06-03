#pragma once
#include <JuceHeader.h>
#include "PedalDefinition.h"
#include "PedalStructures.h"
#include "ResourceManager.h"
#include "ThemeManager.h"
#include "PedalSkinManager.h"

class DrawdioProcessor;

class PedalComponent : public juce::Component
{
public:
    PedalComponent(DrawdioProcessor& processor,
                   int slotIndex,
                   DspModuleType initialType,
                   const ResourceManager& resources,
                   const ThemeManager& theme,
                   PedalSkinManager::PedalSkin skin = PedalSkinManager::PedalSkin::Default);
    ~PedalComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    void setKnobValue(int knobIdx, float value);
    void syncFromProcessor();
    void setSkin(PedalSkinManager::PedalSkin skin);

    juce::Point<float> getInputJackPos() const;
    juce::Point<float> getOutputJackPos() const;

    static const char* typeName(DspModuleType t);
    static juce::String knobLabel(DspModuleType t, int idx);

private:
    void showTypePopup();
    void initKnob(juce::Slider& knob);
    void updateDefinition();

    DrawdioProcessor& audioProcessor;
    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
    int m_slotIndex;
    DspModuleType m_currentType;
    const PedalDefinition* m_definition = nullptr;
    PedalSkinManager::PedalSkin m_skin;

    juce::Slider m_knobs[4];
};
