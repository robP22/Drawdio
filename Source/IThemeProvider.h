#pragma once

#include <JuceHeader.h>
#include <cstdint>

struct PedalStyle
{
    juce::Colour backgroundColour = juce::Colour(0xFF24292A);
    float bodyRadius = 13.0f;
    float faceRadius = 10.0f;
    float lcdRadius = 5.0f;
    float shadowOffsetX = 2.0f;
    float shadowOffsetY = 6.0f;
    float shadowAlpha = 0.38f;
};

class IThemeProvider
{
public:
    virtual ~IThemeProvider() = default;

    virtual juce::Colour editorBackground() const = 0;
    virtual juce::Colour workspaceFallback() const = 0;
    virtual juce::Colour workspaceVignette() const = 0;

    virtual juce::Colour panelTop() const = 0;
    virtual juce::Colour panelBottom() const = 0;
    virtual juce::Colour panelEdge() const = 0;
    virtual juce::Colour insetPanelTop() const = 0;
    virtual juce::Colour insetPanelBottom() const = 0;

    virtual juce::Colour pedalSkin(int slot) const = 0;
    virtual juce::Colour pedalSideBottom() const = 0;
    virtual juce::Colour pedalLcdTop() const = 0;
    virtual juce::Colour pedalLcdBottom() const = 0;
    virtual juce::Colour pedalActiveLed() const = 0;
    virtual juce::Colour pedalInactiveLed() const = 0;

    virtual juce::Colour canvasPixelColour(uint8_t rawColor) const = 0;
    virtual juce::Colour canvasSurface() const = 0;
    virtual juce::Colour canvasGrid() const = 0;

    virtual juce::Colour paletteSelectionFill(juce::Colour paintColour) const = 0;
    virtual juce::Colour paletteSelectionOutline() const = 0;
    virtual juce::Colour paletteHoverOutline() const = 0;

    virtual juce::Colour drawButtonAccent() const = 0;
    virtual juce::Colour undoButtonAccent() const = 0;
    virtual juce::Colour clearButtonAccent() const = 0;
    virtual juce::Colour cableColour() const = 0;

    virtual const PedalStyle& pedalStyle() const noexcept = 0;
};