#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include "UI/Theme/IThemeProvider.h"

class ThemeManager final : public IThemeProvider
{
public:
    juce::Colour editorBackground() const override;
    juce::Colour workspaceFallback() const override;
    juce::Colour workspaceVignette() const override;

    juce::Colour panelTop() const override;
    juce::Colour panelBottom() const override;
    juce::Colour panelEdge() const override;
    juce::Colour insetPanelTop() const override;
    juce::Colour insetPanelBottom() const override;

    juce::Colour pedalSkin(int slot) const override;
    juce::Colour pedalSideBottom() const override;
    juce::Colour pedalLcdTop() const override;
    juce::Colour pedalLcdBottom() const override;
    juce::Colour pedalActiveLed() const override;
    juce::Colour pedalInactiveLed() const override;

    juce::Colour canvasPixelColour(uint8_t rawColor) const override;
    juce::Colour canvasSurface() const override;
    juce::Colour canvasGrid() const override;

    juce::Colour paletteSelectionFill(juce::Colour paintColour) const override;
    juce::Colour paletteSelectionOutline() const override;
    juce::Colour paletteHoverOutline() const override;

    juce::Colour drawButtonAccent() const override;
    juce::Colour undoButtonAccent() const override;
    juce::Colour clearButtonAccent() const override;
    juce::Colour cableColour() const override;
    juce::Colour jackHighlightColour() const override;

    const PedalStyle& pedalStyle() const noexcept override { return m_pedalStyle; }

private:
    PedalStyle m_pedalStyle;
};
