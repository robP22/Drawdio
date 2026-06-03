#pragma once

#include <JuceHeader.h>
#include <cstdint>

class ThemeManager final
{
public:
    struct PedalStyle
    {
        float bodyRadius = 13.0f;
        float faceRadius = 10.0f;
        float lcdRadius = 5.0f;
        float shadowOffsetX = 2.0f;
        float shadowOffsetY = 6.0f;
        float shadowAlpha = 0.38f;
    };

    static const ThemeManager& getDefault();

    juce::Colour editorBackground() const;
    juce::Colour workspaceFallback() const;
    juce::Colour workspaceVignette() const;

    juce::Colour panelTop() const;
    juce::Colour panelBottom() const;
    juce::Colour panelEdge() const;
    juce::Colour insetPanelTop() const;
    juce::Colour insetPanelBottom() const;

    juce::Colour pedalSkin(int slot) const;
    juce::Colour pedalSideBottom() const;
    juce::Colour pedalLcdTop() const;
    juce::Colour pedalLcdBottom() const;
    juce::Colour pedalActiveLed() const;
    juce::Colour pedalInactiveLed() const;

    juce::Colour canvasPixelColour(uint8_t rawColor) const;
    juce::Colour canvasSurface() const;
    juce::Colour canvasGrid() const;

    juce::Colour paletteSelectionFill(juce::Colour paintColour) const;
    juce::Colour paletteSelectionOutline() const;
    juce::Colour paletteHoverOutline() const;

    juce::Colour drawButtonAccent() const;
    juce::Colour undoButtonAccent() const;
    juce::Colour clearButtonAccent() const;
    juce::Colour cableColour() const;

    const PedalStyle& pedalStyle() const noexcept { return m_pedalStyle; }

private:
    PedalStyle m_pedalStyle;
};
