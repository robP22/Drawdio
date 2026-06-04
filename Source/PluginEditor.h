#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "PedalboardGrid.h"
#include "PixelCanvasComponent.h"
#include "PluginProcessor.h"
#include "ResourceManager.h"
#include "ThemeManager.h"
#include "CanvasRoutingManager.h"

// Background components for left (wood) and right (pedalboard) halves
class WoodGrainBackground : public juce::Component
{
public:
    WoodGrainBackground(const ResourceManager& resources, const ThemeManager& theme);
    void paint(juce::Graphics& g) override;
private:
    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
};

class PedalboardBackground : public juce::Component
{
public:
    PedalboardBackground(const ResourceManager& resources, const ThemeManager& theme);
    void paint(juce::Graphics& g) override;
private:
    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
};

class ColorPalette : public juce::Component
{
public:
    using ColorCallback = std::function<void(PixelCanvasComponent::PixelColor)>;

    ColorPalette(const ResourceManager& resources, const ThemeManager& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setSelectedColor(PixelCanvasComponent::PixelColor color);
    void setOnColorSelected(ColorCallback cb) { m_onColorSelected = std::move(cb); }

private:
    struct PaintBlob
    {
        PixelCanvasComponent::PixelColor color;
        juce::Rectangle<float> bounds;
    };

    int hitTestBlob(juce::Point<float> position) const;

    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
    std::array<PaintBlob, 5> m_blobs;
    PixelCanvasComponent::PixelColor m_selectedColor = PixelCanvasComponent::PixelColor::Red;
    int m_hoveredBlob = -1;
    ColorCallback m_onColorSelected;
};

class CanvasTools : public juce::Component
{
public:
    explicit CanvasTools(const ThemeManager& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setOnUndo(std::function<void()> cb) { m_onUndo = std::move(cb); }
    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }

private:
    void styleButton(juce::TextButton& button, juce::Colour accent);

    const ThemeManager& m_theme;
    juce::TextButton m_undoButton { "Undo" };
    juce::TextButton m_clearButton { "Clear" };

    std::function<void()> m_onUndo;
    std::function<void()> m_onClear;
};

class CanvasModule : public juce::Component
{
public:
    CanvasModule(const ResourceManager& resources, const ThemeManager& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;

    PixelCanvasComponent& getPixelCanvas() { return m_pixelCanvas; }
    const PixelCanvasComponent& getPixelCanvas() const { return m_pixelCanvas; }

    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }
    void refreshStatus();

private:
    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
    PixelCanvasComponent m_pixelCanvas;
    ColorPalette m_palette;
    CanvasTools m_tools;

    std::function<void()> m_onClear;
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
