#pragma once
#include <JuceHeader.h>
#include <functional>
#include "PixelCanvasComponent.h"
#include "IThemeProvider.h"
#include "ColorPalette.h"

class CanvasModule : public juce::Component
{
public:
    CanvasModule(const ResourceManager& resources, const IThemeProvider& theme);

    void resized() override;

    PixelCanvasComponent& getPixelCanvas();
    const PixelCanvasComponent& getPixelCanvas() const;

    void setOnClear(std::function<void()> cb);

private:
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
    PixelCanvasComponent m_pixelCanvas;
    ColorPalette m_palette;

    std::function<void()> m_onClear;
};
