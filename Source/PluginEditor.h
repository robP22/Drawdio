#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "UI/Canvas/PixelCanvasComponent.h"
#include "UI/Canvas/ColorPalette.h"
#include "PedalboardGrid.h"
#include "Resources/ResourceManager.h"
#include "UI/Theme/ThemeManager.h"
#include "UI/Theme/IThemeProvider.h"
#include "State/CanvasRoutingManager.h"
#include "UI/Controls/BottomControlBar.h"
#include "State/AutomationCompiler.h"
#include "State/AutomationPlayer.h"
#include "UI/EditorSyncController.h"
#include "UI/WoodGrainBackground.h"
#include "UI/Pedalboard/PedalboardBackground.h"

class DrawdioProcessor;

class DrawdioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrawdioProcessorEditor(DrawdioProcessor&);
    ~DrawdioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void enterManualMode();
    void exitManualMode();
    void clearManualState();
    void savePreset();
    void loadPreset();
    void importImage();

private:
    void triggerRecompile();
    void timerCallback() override;

    DrawdioProcessor& audioProcessor;
    ResourceManager m_resourceManager;
    ThemeManager m_themeImpl;
    const IThemeProvider& m_theme;
    CanvasRoutingManager m_routingManager;
    WoodGrainBackground m_woodGrainBackground;
    PedalboardBackground m_pedalboardBackground;
    PixelCanvasComponent m_pixelCanvas;
    ColorPalette m_palette;
    PedalboardGrid m_pedalboardGrid;
    BottomControlBar m_bottomBar;
    AutomationCompiler m_automationCompiler;
    AutomationPlayer m_automationPlayer;
    EditorSyncController m_syncController;
    int m_lastPedalboardWidth = -1;
    float m_pedalTopRatio = 0.0f;
    float m_pedalBottomRatio = 0.0f;
    float m_paletteTopRatio = 0.0f;
    float m_paletteBottomRatio = 0.0f;
};
