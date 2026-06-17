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

class HamburgerButton : public juce::Button
{
public:
    HamburgerButton() : juce::Button("Hamburger") {}

    void paintButton(juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced(9.0f);
        const float lh = b.getHeight() * 0.13f;
        const float y[3] = { b.getY(), b.getCentreY() - lh * 0.5f, b.getBottom() - lh };

        g.setColour(juce::Colours::black.withAlpha(0.45f));
        for (int i = 0; i < 3; ++i)
            g.fillRoundedRectangle(b.getX() + 1.5f, y[i] + 1.5f, b.getWidth(), lh, 2.0f);

        g.setColour(over ? juce::Colours::white : juce::Colours::white.withAlpha(0.85f));
        for (int i = 0; i < 3; ++i)
            g.fillRoundedRectangle(b.getX(), y[i], b.getWidth(), lh, 2.0f);
    }
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
    void showHamburgerMenu();

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
    bool m_needsRepaint = false;
    HamburgerButton m_hamburgerButton;
};
