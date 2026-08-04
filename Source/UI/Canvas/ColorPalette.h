#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>
#include "UI/Theme/IThemeProvider.h"
#include "Core/Contracts/IResourceProvider.h"
#include "UI/Canvas/ArcButton.h"

class ColorPalette : public juce::Component
{
public:
    using ColorCallback = std::function<void(uint8_t)>;

    ColorPalette(const IResourceProvider& resources, const IThemeProvider& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setSelectedColor(uint8_t color);
    void setOnColorSelected(ColorCallback cb) { m_onColorSelected = std::move(cb); }
    void setOnUndo(std::function<void()> cb) { m_onUndo = std::move(cb); }
    void setOnRedo(std::function<void()> cb) { m_onRedo = std::move(cb); }
    void setOnClear(std::function<void()> cb) { m_onClear = std::move(cb); }
    void setOnFill(std::function<void(bool)> cb) { m_onFill = std::move(cb); }
    void setOnBrushSize(std::function<void(float)> cb) { m_onBrushSize = std::move(cb); }
    void setOnPartyMode(std::function<void(bool)> cb) { m_onPartyMode = std::move(cb); }
    void setOnEraser(std::function<void(bool)> cb) { m_onEraser = std::move(cb); }
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

    const IResourceProvider& m_resources;
    const IThemeProvider& m_theme;
    std::array<PaintBlob, 12> m_blobs;
    uint8_t m_selectedColor = 3;
    int m_hoveredBlob = -1;
    ColorCallback m_onColorSelected;
    ArcButton m_partyButton;
    ArcButton m_undoButton;
    ArcButton m_redoButton;
    ArcButton m_fillButton;
    ArcButton m_sizeButton;
    ArcButton m_eraserButton;
    ArcButton m_clearButton;
    std::function<void()> m_onUndo;
    std::function<void()> m_onRedo;
    std::function<void()> m_onClear;
    std::function<void(bool)> m_onFill;
    std::function<void(float)> m_onBrushSize;
    std::function<void(bool)> m_onPartyMode;
    std::function<void(bool)> m_onEraser;
    uint8_t m_eraserSavedColor = 3;
    std::array<float, 4> m_brushSizes { 0.75f, 1.5f, 2.5f, 4.0f };
    int m_currentBrushIndex = 0;
    float m_imageCenterX = 0.0f;
    float m_imageVerticalShift = 0.0f;
    float m_contentCenterOffset = 0.0f;
};
