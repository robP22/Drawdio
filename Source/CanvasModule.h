#pragma once
#include <JuceHeader.h>
#include <functional>
#include "ColorPalette.h"
#include "PixelCanvasComponent.h"

class ResourceManager;
class ThemeManager;

class CanvasModule : public juce::Component
{
public:
    CanvasModule(const ResourceManager& resources, const ThemeManager& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;

    PixelCanvasComponent& getPixelCanvas();
    const PixelCanvasComponent& getPixelCanvas() const;

    void setOnClear(std::function<void()> cb);
    void refreshStatus();

private:
    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
    PixelCanvasComponent m_pixelCanvas;
    ColorPalette m_palette;

    std::function<void()> m_onClear;
};