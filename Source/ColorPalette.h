#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>

class ResourceManager;
class ThemeManager;

class ColorPalette : public juce::Component
{
public:
    using ColorCallback = std::function<void(class PixelCanvasComponent::PixelColor)>;

    ColorPalette(const ResourceManager& resources, const ThemeManager& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setSelectedColor(class PixelCanvasComponent::PixelColor color);
    void setOnColorSelected(ColorCallback cb) { m_onColorSelected = std::move(cb); }
    void setOnUndo(std::function<void()> cb) { m_onUndo = std::move(cb); }
    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }

private:
    void styleButton(juce::TextButton& button, juce::Colour accent);

    struct PaintBlob
    {
        class PixelCanvasComponent::PixelColor color;
        juce::Rectangle<float> bounds;
    };

    int hitTestBlob(juce::Point<float> position) const;

    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
    std::array<PaintBlob, 5> m_blobs;
    class PixelCanvasComponent::PixelColor m_selectedColor = class PixelCanvasComponent::PixelColor::Red;
    int m_hoveredBlob = -1;
    ColorCallback m_onColorSelected;
    juce::TextButton m_undoButton { "Undo" };
    juce::TextButton m_clearButton { "Clear" };
    std::function<void()> m_onUndo;
    std::function<void()> m_onClear;
};