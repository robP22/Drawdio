#pragma once
#include <JuceHeader.h>
#include <functional>

class ResourceManager;
class ThemeManager;

class CanvasModule : public juce::Component
{
public:
    CanvasModule(const ResourceManager& resources, const ThemeManager& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;

    class PixelCanvasComponent& getPixelCanvas();
    const class PixelCanvasComponent& getPixelCanvas() const;

    void setOnClear(std::function<void()> cb);
    void refreshStatus();

private:
    const ResourceManager& m_resources;
    const ThemeManager& m_theme;
    class PixelCanvasComponent m_pixelCanvas;
    class ColorPalette m_palette;

    std::function<void()> m_onClear;
};