#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "PixelCanvasComponent.h"
#include "ColorPalette.h"
#include "PedalboardGrid.h"
#include "ResourceManager.h"
#include "ThemeManager.h"
#include "IThemeProvider.h"
#include "CanvasRoutingManager.h"
#include "BottomControlBar.h"
#include "AutomationCompiler.h"
#include "AutomationPlayer.h"

class DrawdioProcessor;
class WoodGrainBackground;
class PedalboardBackground;

class DrawdioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrawdioProcessorEditor(DrawdioProcessor&);
    ~DrawdioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void showHamburgerMenu();
    void savePreset();
    void loadPreset();

private:
    void triggerRecompile();
    void checkForUpdates();
    void syncCompiledKnobs(bool& needsRepaint);
    void syncKnobAutomation();
    void syncAutomation();
    void refreshRoutingFromConfig();
    void timerCallback() override;

    DrawdioProcessor& audioProcessor;
    ResourceManager m_resourceManager;
    ThemeManager m_theme;
    CanvasRoutingManager m_routingManager;
    std::unique_ptr<WoodGrainBackground> m_woodGrainBackground;
    std::unique_ptr<PedalboardBackground> m_pedalboardBackground;
    PixelCanvasComponent m_pixelCanvas;
    ColorPalette m_palette;
    PedalboardGrid m_pedalboardGrid;
    std::vector<uint8_t> m_lastRoutingOrder;
    uint32_t m_seenConfigRevision = 0;
    bool m_needsRepaint = false;
    BottomControlBar m_bottomBar;
    AutomationCompiler m_automationCompiler;
    AutomationPlayer m_automationPlayer;
};
