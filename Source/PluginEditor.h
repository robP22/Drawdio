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

class PaletteTools : public juce::Component
{
public:
    PaletteTools();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setOnUndo(std::function<void()> cb) { m_onUndo = std::move(cb); }
    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }

private:
    void loadTexture();

    juce::Image m_texture;
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
    PaletteTools m_paletteTools;

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
