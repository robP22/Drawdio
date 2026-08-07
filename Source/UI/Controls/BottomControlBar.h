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
    void updateBarsButton(int bars) { m_barsBtn.setButtonText("Length: " + juce::String(bars) + " bar" + (bars > 1 ? "s" : "")); }
    void updateManualButton(bool active) { m_manualBtn.setButtonText(active ? "Routing: Canvas" : "Routing: Manual"); }
    void syncPedalNames();
    void syncGainKnobs();
    void tick();
    std::function<void(bool)> onManualModeToggled;
    std::function<void()> onPresetSave;
    std::function<void()> onPresetLoad;
    std::function<void()> onPresetImport;

private:
    IBottomBarModel& m_model;
    const IResourceProvider& m_resources;
    juce::TextButton m_barsBtn{"Length: 1 bar"};
    juce::TextButton m_manualBtn{"Routing: Manual"};
    juce::TextButton m_saveBtn{"Save"};
    juce::TextButton m_loadBtn{"Load"};
    juce::TextButton m_importBtn{"Import"};
    std::unique_ptr<SpriteKnob> m_inputKnob;
    std::unique_ptr<SpriteKnob> m_outputKnob;
    AutomationDisplay m_automationDisplay;
    std::array<std::unique_ptr<MixerStrip>, PedalSlotCount> m_mixerStrips;
};
