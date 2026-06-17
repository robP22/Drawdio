#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>
#include "IThemeProvider.h"
#include "ResourceManager.h"

class ColorPalette : public juce::Component
{
public:
    using ColorCallback = std::function<void(uint8_t)>;

    ColorPalette(const ResourceManager& resources, const IThemeProvider& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setSelectedColor(uint8_t color);
    void setOnColorSelected(ColorCallback cb) { m_onColorSelected = std::move(cb); }
    void setOnUndo(std::function<void()> cb) { m_onUndo = std::move(cb); }
    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }
    void setOnFill(std::function<void(bool)> cb) { m_onFill = std::move(cb); }
    void setFillButtonState(bool on) { m_fillButton.setToggleState(on, juce::dontSendNotification); }
    void setOnBrushSize(std::function<void(float)> cb) { m_onBrushSize = std::move(cb); }
    void setImageCenterX(float x) { m_imageCenterX = x; repaint(); }
    void setImageBottomShift(float px) { m_imageVerticalShift = px; repaint(); }
    void setContentCenterOffset(float px) { m_contentCenterOffset = px; resized(); }

private:
    struct PaintBlob
    {
        uint8_t color;
        juce::Rectangle<float> bounds;
    };

    int hitTestBlob(juce::Point<float> position) const;
    void cycleBrushSize();

    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
    std::array<PaintBlob, 10> m_blobs;
    uint8_t m_selectedColor = 3;
    int m_hoveredBlob = -1;
    ColorCallback m_onColorSelected;
    juce::TextButton m_undoButton { "Undo" };
    juce::TextButton m_clearButton { "Clear" };
    juce::TextButton m_fillButton { "Fill" };
    juce::TextButton m_sizeButton { "Size" };
    std::function<void()> m_onUndo;
    std::function<void()> m_onClear;
    std::function<void(bool)> m_onFill;
    std::function<void(float)> m_onBrushSize;
    std::array<float, 3> m_brushSizes { 0.75f, 1.5f, 2.5f };
    int m_currentBrushIndex = 0;
    float m_imageCenterX = 0.0f;
    float m_imageVerticalShift = 0.0f;
    float m_contentCenterOffset = 0.0f;
};