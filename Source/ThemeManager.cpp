#include "ThemeManager.h"
#include "PedalStructures.h"

juce::Colour ThemeManager::editorBackground() const { return juce::Colour(0xFF151719); }
juce::Colour ThemeManager::workspaceFallback() const { return juce::Colour(0xFF20150D); }
juce::Colour ThemeManager::workspaceVignette() const { return juce::Colours::black.withAlpha(0.38f); }

juce::Colour ThemeManager::panelTop() const { return juce::Colour(0xFF34393D); }
juce::Colour ThemeManager::panelBottom() const { return juce::Colour(0xFF181B1E); }
juce::Colour ThemeManager::panelEdge() const { return juce::Colour(0xFF3B4043); }
juce::Colour ThemeManager::insetPanelTop() const { return juce::Colour(0xFF252A2D); }
juce::Colour ThemeManager::insetPanelBottom() const { return juce::Colour(0xFF111315); }

juce::Colour ThemeManager::pedalSkin(int slot) const
{
    static constexpr uint32_t colours[] {
        0xFF3B5A74, 0xFF6E3E49, 0xFF4D6846,
        0xFF6B603A, 0xFF584E75, 0xFF5E6266
    };
    constexpr auto colourCount = static_cast<int>(sizeof(colours) / sizeof(colours[0]));
    return juce::Colour(colours[static_cast<size_t>(slot % colourCount)]);
}

juce::Colour ThemeManager::pedalSideBottom() const { return juce::Colour(0xFF0F1113); }
juce::Colour ThemeManager::pedalLcdTop() const { return juce::Colour(0xFF233034); }
juce::Colour ThemeManager::pedalLcdBottom() const { return juce::Colour(0xFF070909); }
juce::Colour ThemeManager::pedalActiveLed() const { return juce::Colour(0xFF50F07E); }
juce::Colour ThemeManager::pedalInactiveLed() const { return juce::Colour(0xFF344039); }

juce::Colour ThemeManager::canvasPixelColour(uint8_t rawColor) const
{
    switch (rawColor)
    {
        case 4: return juce::Colour(0xFFE8E5DC);
        case 3: return juce::Colour(0xFFE54235);
        case 2: return juce::Colour(0xFF2BBE65);
        case 1: return juce::Colour(0xFF2F73D8);
        case 6: return juce::Colour(0xFFFFD700);   // Yellow
        case 7: return juce::Colour(0xFF8B4513);   // Brown
        case 8: return juce::Colour(0xFF800080);   // Purple
        case 9: return juce::Colour(0xFF808080);   // Grey
        case 10: return juce::Colour(0xFFFF69B4);  // Pink
        default: return juce::Colour(0xFF121212);
    }
}

juce::Colour ThemeManager::canvasSurface() const { return juce::Colour(0xFFD6D3CA); }
juce::Colour ThemeManager::canvasGrid() const { return juce::Colours::black; }

juce::Colour ThemeManager::paletteSelectionFill(juce::Colour paintColour) const
{
    return paintColour.withAlpha(0.30f);
}

juce::Colour ThemeManager::paletteSelectionOutline() const
{
    return juce::Colours::white.withAlpha(0.42f);
}

juce::Colour ThemeManager::paletteHoverOutline() const
{
    return juce::Colours::white.withAlpha(0.16f);
}

juce::Colour ThemeManager::drawButtonAccent() const { return juce::Colour(0xFF47C9A2); }
juce::Colour ThemeManager::undoButtonAccent() const { return juce::Colour(0xFFC7B067); }
juce::Colour ThemeManager::clearButtonAccent() const { return juce::Colour(0xFFD75B4F); }
juce::Colour ThemeManager::cableInColour() const { return juce::Colour(0xFF2E8B57); }
juce::Colour ThemeManager::cableOutColour() const { return juce::Colour(0xFF1565C0); }
