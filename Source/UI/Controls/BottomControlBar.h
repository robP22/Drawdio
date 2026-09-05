#pragma once
#include <JuceHeader.h>
#include <functional>
#include "UI/Theme/HeaderPill.h"
#include "UI/Theme/SplitPill.h"
#include "UI/Controls/MixerStrip.h"
#include "UI/Controls/AutomationDisplay.h"
#include "UI/Controls/SpriteKnob.h"
#include "Resources/ScaledAssetProvider.h"
#include "Core/DrawdioConstants.h"
#include "UI/EditorState.h"

class BottomControlBar : public juce::Component
{
public:
    struct Actions
    {
        std::function<void(float)> setInputGain;
        std::function<void(float)> setOutputGain;
        std::function<void(int, float)> setPedalGain;
        std::function<void(int)> setBarCount;
        std::function<void(int)> setSectionStart;
    };

    BottomControlBar(const ScaledAssetProvider& assets,
                     Actions actions);
    void resized() override;
    void paint(juce::Graphics& g) override;
    AutomationDisplay& getAutomationDisplay() { return m_automationDisplay; }
    void updateBarsButton(int bars) { m_barsPill.setPillValue(juce::String(bars) + " bar" + (bars > 1 ? "s" : "")); }
    void syncPedalNames();
    void tick();
    void setViewState(const EditorUiSnapshot& state);
    void setPedalPeak(int slot, float peak);
    void setManualMode(bool manual);
    std::function<void()> onPresetSave;
    std::function<void()> onPresetImport;
    std::function<void()> onImageImport;
    std::function<void()> onImageExport;

private:
    const ScaledAssetProvider& m_assets;
    Actions m_actions;
    HeaderPill m_barsPill;
    SplitPill m_presetPill;
    SplitPill m_imagePill;
    std::unique_ptr<SpriteKnob> m_inputKnob;
    std::unique_ptr<SpriteKnob> m_outputKnob;
    AutomationDisplay m_automationDisplay;
    std::array<std::unique_ptr<MixerStrip>, PedalSlotCount> m_mixerStrips;
    std::array<DspModuleType, PedalSlotCount> m_pedalTypes{};
};
