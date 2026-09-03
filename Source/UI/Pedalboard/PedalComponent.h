#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>

#include "Core/DspModuleType.h"
#include "State/PedalDefinition.h"
#include "Core/Contracts/IResourceProvider.h"
#include "Resources/ScaledAssetProvider.h"
#include "UI/Theme/IThemeProvider.h"
#include "UI/Controls/SpriteKnob.h"
#include "Core/Contracts/IComponentBounds.h"

class PedalComponent : public juce::Component, public IComponentBounds
{
public:
    struct Actions
    {
        std::function<void(int, DspModuleType)> setType;
        std::function<void(int, int, float, float)> setKnob;
        std::function<void(int, int, bool)> setLink;
        std::function<void(int, int, float, float)> setLinkRange;
    };

    PedalComponent(int slotIndex,
                   DspModuleType initialType,
                   const IResourceProvider& resources,
                   const ScaledAssetProvider& assets,
                   const IThemeProvider& theme,
                   Actions actions);
    ~PedalComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    void setKnobValue(int knobIdx, float value);
    float snapValue(int knobIdx, float value) const
    {
        return PedalDefinitions::snapValue(m_currentType, knobIdx, value);
    }
    void syncType(DspModuleType type);
    void setKnobLinked(int knobIdx, bool linked);
    void setKnobLinkRange(int knobIdx, float rMin, float rMax);
    juce::Rectangle<int> getBounds() const override;
    void setBounds(juce::Rectangle<int>) override;
    juce::Point<float> getInputJackPos() const override;
    juce::Point<float> getOutputJackPos() const override;

private:
    void showTypePopup();
    void updateKnobBounds();
    void applyKnobLayout();
    void onKnobDragStart(int knobIdx, float value);
    void onKnobValueChanged(int knobIdx, float value);
    void onKnobRightClick(int knobIdx);
    juce::Rectangle<float> getLabelArea() const;

    static constexpr int kKnobCount = 4;

    const IResourceProvider& m_resources;
    const ScaledAssetProvider& m_assets;
    const IThemeProvider& m_theme;
    Actions m_actions;
    int m_slotIndex;
    DspModuleType m_currentType;
    const PedalDefinition* m_definition = nullptr;
    std::array<juce::Rectangle<float>, kKnobCount> m_knobBounds{};
    std::array<bool, kKnobCount> m_linked{};
    std::array<float, kKnobCount> m_linkMins{};
    std::array<float, kKnobCount> m_linkMaxs{};
    std::array<std::unique_ptr<SpriteKnob>, kKnobCount> m_knobs;
    juce::Image m_ledScaled[2];
    int m_ledScaledSize = 0;
    float m_knobDragStartValues[kKnobCount]{};
    juce::Font m_lcdFont { juce::FontOptions(10.0f) };
    juce::Font m_labelFont { juce::FontOptions(8.0f) };
};
