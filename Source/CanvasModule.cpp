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

    m_palette.setOnFill([this](bool active)
    {
        m_pixelCanvas.setFillMode(active);
    });

    m_palette.setOnBrushSize([this](float radius)
    {
        m_pixelCanvas.setBrushRadius(radius);
    });

    m_palette.setOnPartyMode([this](bool on)
    {
        m_pixelCanvas.setPartyModeEnabled(on);
    });

    m_pixelCanvas.setOnColorChanged([this](PixelCanvasComponent::PixelColor color)
    {
        m_palette.setSelectedColor(static_cast<uint8_t>(color));
    });

    m_pixelCanvas.setOnFillModeChanged([this](bool active)
    {
        m_palette.setFillButtonState(active);
    });

    m_pixelCanvas.setCurrentColor(PixelCanvasComponent::PixelColor::Red);
}

void CanvasModule::resized()
{
    auto full = getLocalBounds();

    const int paletteH = juce::roundToInt(full.getHeight() * GridLayout::PaletteHeightRatio);
    auto paletteArea = full.removeFromBottom(paletteH);

    auto canvasArea = full;
    canvasArea.setBottom(paletteArea.getY());
    const int squareSize = canvasArea.getHeight();
    const auto pixelCanvasBounds = canvasArea.withSizeKeepingCentre(squareSize, squareSize);
    m_pixelCanvas.setBounds(pixelCanvasBounds);
    m_pixelCanvas.setCanvasTopOffset(m_canvasTopOffset);

    m_palette.setBounds(paletteArea);
    m_palette.setImageBottomShift(static_cast<float>(m_paletteShift));
    m_palette.setContentCenterOffset(static_cast<float>(m_paletteCenterOffset));

    const float canvasScale = GridLayout::CanvasScaleRatio;
    const float canvasW = pixelCanvasBounds.getWidth() * canvasScale;
    const float canvasCX = pixelCanvasBounds.getX()
        + (pixelCanvasBounds.getWidth() - canvasW) * 0.5f
        + pixelCanvasBounds.getWidth() * GridLayout::CanvasCenterXShiftRatio
        + canvasW * 0.5f;
    m_palette.setImageCenterX(canvasCX - paletteArea.getX());
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

void CanvasModule::setVerticalOffsets(int canvasTopPx, int paletteShiftPx, int paletteCenterOffset)
{
    m_canvasTopOffset = canvasTopPx;
    m_paletteShift = paletteShiftPx;
    m_paletteCenterOffset = paletteCenterOffset;
    resized();
}

