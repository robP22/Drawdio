#pragma once
#include <JuceHeader.h>
#include <functional>
#include "UI/Controls/MixerStrip.h"
#include "UI/Controls/AutomationDisplay.h"
#include "UI/Controls/SpriteKnob.h"
#include "Core/Contracts/IResourceProvider.h"
#include "Core/DrawdioConstants.h"
#include "Core/Contracts/ProcessorInterfaces.h"

class BottomControlBar : public juce::Component
{
public:
    BottomControlBar(IBottomBarModel& model, const IResourceProvider& resources);
    void resized() override;
    void paint(juce::Graphics& g) override;
    AutomationDisplay& getAutomationDisplay() { return m_automationDisplay; }
    void updateBarsButton(int bars) { m_barsBtn.setButtonText(juce::String(bars) + " bar" + (bars > 1 ? "s" : "")); }
    void updateManualButton(bool active) { m_manualBtn.setButtonText(active ? "Canvas" : "Manual"); }
    void syncPedalNames();
    void syncGainKnobs();
    std::function<void(bool)> onManualModeToggled;

private:
    IBottomBarModel& m_model;
    const IResourceProvider& m_resources;
    juce::TextButton m_barsBtn{"1 bar"};
    juce::TextButton m_manualBtn{"Manual"};
    std::unique_ptr<SpriteKnob> m_inputKnob;
    std::unique_ptr<SpriteKnob> m_outputKnob;
    AutomationDisplay m_automationDisplay;
    std::array<std::unique_ptr<MixerStrip>, PedalSlotCount> m_mixerStrips;
};
