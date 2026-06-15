#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <vector>

#include "CanvasModule.h"
#include "PedalboardGrid.h"
#include "PluginProcessor.h"
#include "ResourceManager.h"
#include "ThemeManager.h"
#include "IThemeProvider.h"
#include "CanvasRoutingManager.h"

// Background components for left (wood) and right (pedalboard) halves
class WoodGrainBackground : public juce::Component
{
public:
    WoodGrainBackground(const ResourceManager& resources, const IThemeProvider& theme);
    void paint(juce::Graphics& g) override;
private:
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
};

class PedalboardBackground : public juce::Component
{
public:
    PedalboardBackground(const ResourceManager& resources, const IThemeProvider& theme);
    void paint(juce::Graphics& g) override;
private:
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
};

class DrawdioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrawdioProcessorEditor(DrawdioProcessor&);
    ~DrawdioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void triggerRecompile();
    void checkForUpdates();  // Change-driven update check
    void refreshRoutingFromConfig();  // Sync routing from current config
    void timerCallback() override;  // JUCE Timer callback

    DrawdioProcessor& audioProcessor;
    ResourceManager m_resourceManager;
    ThemeManager m_theme;
    CanvasRoutingManager m_routingManager;
    WoodGrainBackground m_woodGrainBackground;
    PedalboardBackground m_pedalboardBackground;
    CanvasModule m_canvasModule;
    PedalboardGrid m_pedalboardGrid;
    std::vector<uint8_t> m_lastRoutingOrder;
    uint32_t m_seenConfigRevision = 0;
};
