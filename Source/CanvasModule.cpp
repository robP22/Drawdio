#include "CanvasModule.h"
#include "GridLayout.h"
#include "ColorPalette.h"
#include "PixelCanvasComponent.h"
#include "ResourceManager.h"
#include "ThemeManager.h"

CanvasModule::CanvasModule(const ResourceManager& resources, const IThemeProvider& theme)
    : m_resources(resources),
      m_theme(theme),
      m_pixelCanvas(resources, theme),
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

void CanvasModule::resized()
{
    auto full = getLocalBounds();
    const int outerMargin = juce::roundToInt(full.getWidth() * GridLayout::CanvasOuterMarginRatio);
    const int vertMargin = juce::roundToInt(full.getHeight() * GridLayout::CanvasVerticalMarginRatio);
    auto area = full.reduced(outerMargin, vertMargin);

    const int paletteH = juce::roundToInt(full.getHeight() * GridLayout::PaletteHeightRatio);
    auto paletteArea = full.removeFromBottom(paletteH);

    auto canvasArea = full;
    canvasArea.setBottom(paletteArea.getY());
    const int squareSize = canvasArea.getHeight();
    const auto pixelCanvasBounds = canvasArea.withSizeKeepingCentre(squareSize, squareSize);
    m_pixelCanvas.setBounds(pixelCanvasBounds);
    m_palette.setBounds(paletteArea);
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

