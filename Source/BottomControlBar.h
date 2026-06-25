#pragma once
#include <JuceHeader.h>
#include "MixerStrip.h"
#include "PluginProcessor.h"
#include "AutomationDisplay.h"
#include "SpriteKnob.h"
#include "ResourceManager.h"

class DrawdioProcessorEditor;

class BottomControlBar : public juce::Component
{
public:
    BottomControlBar(DrawdioProcessorEditor* editor, DrawdioProcessor& proc,
                     const ResourceManager& resources);
    void resized() override;
    void paint(juce::Graphics& g) override;
    AutomationDisplay& getAutomationDisplay() { return m_automationDisplay; }
    void updateBarsButton(int bars) { m_barsBtn.setButtonText(juce::String(bars) + " bar" + (bars > 1 ? "s" : "")); }
    void updateManualButton(bool active) { m_manualBtn.setButtonText(active ? "Canvas" : "Manual"); }
    void syncPedalNames();
    void syncGainKnobs();

private:
    DrawdioProcessorEditor* m_editor;
    DrawdioProcessor& m_processor;
    const ResourceManager& m_resources;
    juce::TextButton m_barsBtn{"1 bar"};
    juce::TextButton m_manualBtn{"Manual"};
    std::unique_ptr<SpriteKnob> m_inputKnob;
    std::unique_ptr<SpriteKnob> m_outputKnob;
    AutomationDisplay m_automationDisplay;
    std::array<std::unique_ptr<MixerStrip>, PedalSlotCount> m_mixerStrips;
};
