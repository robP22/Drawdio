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

    juce::TextButton m_undoButton { "Undo" };
    juce::TextButton m_clearButton { "Clear" };

    std::function<void()> m_onUndo;
    std::function<void()> m_onClear;
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

private:
    PixelCanvasComponent m_pixelCanvas;
    ColorPalette m_palette;
    CanvasTools m_tools;

    std::function<void()> m_onClear;
};

class DrawdioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DrawdioProcessorEditor(DrawdioProcessor&);
    ~DrawdioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void triggerRecompile();

    DrawdioProcessor& audioProcessor;
    juce::Image m_woodBackground;
    CanvasModule m_canvasModule;
    PedalboardCanvas m_pedalboardCanvas;
};
