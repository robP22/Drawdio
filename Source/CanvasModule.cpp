#include "CanvasModule.h"
#include "ColorPalette.h"
#include "PixelCanvasComponent.h"
#include "ResourceManager.h"
#include "ThemeManager.h"

namespace Layout
{
constexpr int CanvasOuterMargin   = 22;
constexpr int CanvasVerticalMargin = 20;
constexpr float PaletteHeightRatio = 0.26f;
}

CanvasModule::CanvasModule(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme),
      m_pixelCanvas(theme),
      m_palette(resources, theme)
{
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);

    m_palette.setOnColorSelected([this](uint8_t color)
    {
        m_pixelCanvas.setCurrentColor(static_cast<PixelCanvasComponent::PixelColor>(color));
    });

    m_palette.setOnUndo([this]()
    {
        m_pixelCanvas.undo();
    });

    m_palette.setOnClear([this]()
    {
        if (m_onClear)
            m_onClear();

        m_pixelCanvas.clearCanvas();
    });

    m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Red);
}

void CanvasModule::paint(juce::Graphics&)
{
    // COMMENTED OUT FOR DEBUG - canvas module rendering disabled
}

void CanvasModule::resized()
{
    auto full = getLocalBounds();
    auto area = full.reduced(Layout::CanvasOuterMargin, Layout::CanvasVerticalMargin);

    const int paletteH = juce::roundToInt(full.getHeight() * Layout::PaletteHeightRatio);
    auto paletteArea = full.removeFromBottom(paletteH);

    auto canvasArea = full;
    canvasArea.setBottom(paletteArea.getY());
    const int squareSize = canvasArea.getHeight();
    // Properly center the canvas using withSizeKeepingCentre only
    const auto pixelCanvasBounds = canvasArea.withSizeKeepingCentre(squareSize, squareSize);
    m_pixelCanvas.setBounds(pixelCanvasBounds);
    m_palette.setBounds(paletteArea.withTrimmedLeft(pixelCanvasBounds.getX()).withTrimmedRight(3).withTrimmedTop(2));
}

PixelCanvasComponent& CanvasModule::getPixelCanvas()
{
    return m_pixelCanvas;
}

const PixelCanvasComponent& CanvasModule::getPixelCanvas() const
{
    return m_pixelCanvas;
}

void CanvasModule::setOnClear(std::function<void()> cb)
{
    m_onClear = std::move(cb);
}

void CanvasModule::refreshStatus()
{
    // Status display removed - no-op
}