#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "PedalboardBackground.h"
#include "PixelCanvasComponent.h"
#include "PluginProcessor.h"

class PaletteTools : public juce::Component
{
public:
    using ColorCallback = std::function<void(PixelCanvasComponent::PixelColor)>;

    PaletteTools();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setOnColorSelected(ColorCallback cb) { m_onColorSelected = std::move(cb); }
    void setOnUndo(std::function<void()> cb) { m_onUndo = std::move(cb); }
    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }

private:
    void loadTexture();
    int hitTestColor(juce::Point<float> pos) const;

    juce::Image m_texture;
    std::array<juce::Rectangle<float>, 5> m_colorSlots;
    PixelCanvasComponent::PixelColor m_selectedColor = PixelCanvasComponent::PixelColor::Red;
    int m_hoveredColor = -1;

    juce::TextButton m_undoButton { "Undo" };
    juce::TextButton m_clearButton { "Clear" };

    ColorCallback m_onColorSelected;
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
    PedalboardBackground m_pedalboardBackground;
};
