#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "PedalboardCanvas.h"
#include "PixelCanvasComponent.h"
#include "PluginProcessor.h"

class WorkspaceBackground : public juce::Component
{
public:
    WorkspaceBackground();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildCachedBackground();

    juce::Image m_background;
};

class ColorPalette : public juce::Component
{
public:
    using ColorCallback = std::function<void(PixelCanvasComponent::PixelColor)>;

    ColorPalette();

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

    std::array<PaintBlob, 5> m_blobs;
    PixelCanvasComponent::PixelColor m_selectedColor = PixelCanvasComponent::PixelColor::Red;
    int m_hoveredBlob = -1;
    ColorCallback m_onColorSelected;
};

class CanvasTools : public juce::Component
{
public:
    CanvasTools();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setOnUndo(std::function<void()> cb) { m_onUndo = std::move(cb); }
    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }

private:
    void styleButton(juce::TextButton& button, juce::Colour accent);

    juce::TextButton m_drawButton { "Draw" };
    juce::TextButton m_undoButton { "Undo" };
    juce::TextButton m_clearButton { "Clear" };

    std::function<void()> m_onUndo;
    std::function<void()> m_onClear;
};

class CanvasStatusDisplay : public juce::Component
{
public:
    void paint(juce::Graphics& g) override;

    void setSelectedColor(PixelCanvasComponent::PixelColor color);
    void setChangedCellCount(int count);

private:
    PixelCanvasComponent::PixelColor m_selectedColor = PixelCanvasComponent::PixelColor::Red;
    int m_changedCellCount = 0;
};

class CanvasModule : public juce::Component
{
public:
    CanvasModule();

    void paint(juce::Graphics& g) override;
    void resized() override;

    PixelCanvasComponent& getPixelCanvas() { return m_pixelCanvas; }
    const PixelCanvasComponent& getPixelCanvas() const { return m_pixelCanvas; }

    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }
    void refreshStatus();

private:
    PixelCanvasComponent m_pixelCanvas;
    ColorPalette m_palette;
    CanvasTools m_tools;
    CanvasStatusDisplay m_status;

    std::function<void()> m_onClear;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter(juce::String label);

    void paint(juce::Graphics& g) override;
    void setLevel(float level);

private:
    juce::String m_label;
    float m_level = 0.0f;
};

class BottomControlBar : public juce::Component
{
public:
    BottomControlBar();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setMeterLevels(float inputLevel, float outputLevel);

private:
    LevelMeter m_inputMeter { "IN" };
    LevelMeter m_outputMeter { "OUT" };
    juce::Slider m_dryWetSlider;
    juce::ComboBox m_oversamplingSelector;
    juce::ComboBox m_qualitySelector;
};

class DrawdioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit DrawdioProcessorEditor(DrawdioProcessor&);
    ~DrawdioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void triggerRecompile();
    void timerCallback() override;

    DrawdioProcessor& audioProcessor;
    WorkspaceBackground m_workspaceBackground;
    CanvasModule m_canvasModule;
    PedalboardCanvas m_pedalboardCanvas;
    BottomControlBar m_bottomControlBar;
    std::vector<uint8_t> m_lastRoutingOrder;
    uint32_t m_seenConfigRevision = 0;
};
