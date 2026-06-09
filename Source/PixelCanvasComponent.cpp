#include "PixelCanvasComponent.h"
#include "RenderUtils.h"
#include <cmath>
#include <utility>

namespace
{
PixelCanvasComponent::PixelColor pixelFromRaw(uint8_t raw)
{
    switch (raw)
    {
        case 1: return PixelCanvasComponent::PixelColor::Blue;
        case 2: return PixelCanvasComponent::PixelColor::Green;
        case 3: return PixelCanvasComponent::PixelColor::Red;
        case 4: return PixelCanvasComponent::PixelColor::White;
        default: return PixelCanvasComponent::PixelColor::Black;
    }
}
}

PixelCanvasComponent::PixelCanvasComponent(const IThemeProvider& theme)
    : m_theme(theme)
{
    pixels.fill(PixelColor::White);
    m_activeChangeLookup.fill(-1);
    m_activeStroke.reserve(512);
    m_undoStack.reserve(MaxUndoLevels);

    rebuildGridCache();
    rebuildPixelImage();

    setRepaintsOnMouseActivity(true);
}

juce::Colour PixelCanvasComponent::colourForPixel(PixelColor color)
{
    return ThemeManager::getDefault().canvasPixelColour(static_cast<uint8_t>(color));
}

void PixelCanvasComponent::paint(juce::Graphics& g)
{
    if (!m_pixelImage.isValid())
        return;

    // Calculate scaled canvas dimensions using the class constant
    const float canvasW = getWidth() * CanvasScaleRatio;
    const float canvasH = getHeight() * CanvasScaleRatio;
    const float offsetX = (getWidth() - canvasW) / 2.0f;
    const float offsetY = (getHeight() - canvasH) / 2.0f;

    // Draw shadow beneath the canvas layer
    auto bounds = getLocalBounds().toFloat();
    const float size = juce::jmin(getWidth(), getHeight()) * CanvasScaleRatio;
    const auto imageArea = bounds.withSizeKeepingCentre(size, size).toFloat();
    drawCanvasShadow(g, imageArea);

    const float cellW = canvasW / GridSize;
    const float cellH = canvasH / GridSize;

    for (int y = 0; y < GridSize; ++y)
    {
        for (int x = 0; x < GridSize; ++x)
        {
            const auto color = m_theme.canvasPixelColour(static_cast<uint8_t>(pixels[static_cast<size_t>(y * GridSize + x)]));
            g.setColour(color);
            g.fillRect(offsetX + static_cast<float>(x) * cellW,
                       offsetY + static_cast<float>(y) * cellH,
                       cellW,
                       cellH);
        }
    }
}

void PixelCanvasComponent::drawCanvasShadow(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    // Render the shadow effect beneath the canvas layer
    RenderUtils::paintSurfaceDepth(g, bounds);
}

juce::Point<int> PixelCanvasComponent::gridCoordsFromUI(int uiX, int uiY) const
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return {};

    // Calculate canvas position (same as paint())
    const float canvasW = bounds.getWidth() * CanvasScaleRatio;
    const float canvasH = bounds.getHeight() * CanvasScaleRatio;
    const float offsetX = (bounds.getWidth() - canvasW) / 2.0f;
    const float offsetY = (bounds.getHeight() - canvasH) / 2.0f;

    // Adjust coordinates relative to canvas
    const int relX = uiX - static_cast<int>(offsetX);
    const int relY = uiY - static_cast<int>(offsetY);

    const int gridX = (relX * GridSize) / juce::jmax(1, static_cast<int>(canvasW));
    const int gridY = (relY * GridSize) / juce::jmax(1, static_cast<int>(canvasH));
    return { juce::jlimit(0, GridSize - 1, gridX),
             juce::jlimit(0, GridSize - 1, gridY) };
}

juce::Rectangle<int> PixelCanvasComponent::cellBoundsForIndex(int index) const
{
    const int x = index % GridSize;
    const int y = index / GridSize;
    
    // Calculate canvas position (same as paint())
    const float canvasW = getWidth() * CanvasScaleRatio;
    const float canvasH = getHeight() * CanvasScaleRatio;
    const float offsetX = (getWidth() - canvasW) / 2.0f;
    const float offsetY = (getHeight() - canvasH) / 2.0f;

    const float cellW = canvasW / GridSize;
    const float cellH = canvasH / GridSize;

    return juce::Rectangle<float>(offsetX + static_cast<float>(x) * cellW,
                                   offsetY + static_cast<float>(y) * cellH,
                                   cellW + 1.0f,
                                   cellH + 1.0f).getSmallestIntegerContainer();
}

void PixelCanvasComponent::beginStroke()
{
    if (m_activeStrokeOpen)
        return;

    m_activeStrokeOpen = true;
    m_activeStroke.clear();
    m_activeChangeLookup.fill(-1);
}

void PixelCanvasComponent::commitStroke(bool shouldNotify)
{
    if (!m_activeStrokeOpen)
        return;

    if (!m_activeStroke.empty())
    {
        if (m_undoStack.size() == static_cast<size_t>(MaxUndoLevels))
            m_undoStack.erase(m_undoStack.begin());

        m_undoStack.push_back(std::move(m_activeStroke));
    }

    m_activeStroke.clear();
    m_activeStrokeOpen = false;

    if (shouldNotify)
        notifySnapshot();
}

void PixelCanvasComponent::rasterizeLine(juce::Point<int> from, juce::Point<int> to)
{
    int x0 = from.x;
    int y0 = from.y;
    const int x1 = to.x;
    const int y1 = to.y;
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        setPixel(x0, y0, m_currentColor);

        if (x0 == x1 && y0 == y1)
            break;

        const int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void PixelCanvasComponent::setPixel(int gridX, int gridY, PixelColor color)
{
    if (gridX < 0 || gridX >= GridSize || gridY < 0 || gridY >= GridSize)
        return;

    const int index = gridY * GridSize + gridX;
    const auto previous = pixels[static_cast<size_t>(index)];
    if (previous == color)
        return;

    if (m_activeStrokeOpen)
    {
        auto& lookup = m_activeChangeLookup[static_cast<size_t>(index)];
        if (lookup < 0)
        {
            lookup = static_cast<int>(m_activeStroke.size());
            m_activeStroke.push_back({ static_cast<uint16_t>(index), previous, color });
        }
        else
        {
            m_activeStroke[static_cast<size_t>(lookup)].current = color;
        }
    }

    applyPixelValue(index, color);
    repaint(cellBoundsForIndex(index).expanded(1));
}

void PixelCanvasComponent::applyPixelValue(int index, PixelColor color)
{
    const auto previous = pixels[static_cast<size_t>(index)];
    if (previous == PixelColor::Black && color != PixelColor::Black)
        ++m_changedCellCount;
    else if (previous != PixelColor::Black && color == PixelColor::Black)
        --m_changedCellCount;

    pixels[static_cast<size_t>(index)] = color;
    m_gridCache[static_cast<size_t>(index)] = static_cast<uint8_t>(color);
    updatePixelImage(index);
}

void PixelCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    m_drawing = true;
    beginStroke();

    if (m_onPenDown)
        m_onPenDown();

    auto pos = gridCoordsFromUI(event.x, event.y);
    m_lastDrawPos = pos;
    setPixel(pos.x, pos.y, m_currentColor);
}

void PixelCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_drawing)
        return;

    auto pos = gridCoordsFromUI(event.x, event.y);
    rasterizeLine(m_lastDrawPos, pos);
    m_lastDrawPos = pos;
}

void PixelCanvasComponent::mouseUp(const juce::MouseEvent&)
{
    if (!m_drawing)
        return;

    m_drawing = false;
    commitStroke(true);

    if (m_onPenUp)
        m_onPenUp();

    repaint();
}

void PixelCanvasComponent::mouseEnter(const juce::MouseEvent&)
{
    m_mouseInside = true;
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    repaint();
}

void PixelCanvasComponent::mouseExit(const juce::MouseEvent&)
{
    m_mouseInside = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void PixelCanvasComponent::clearCanvas()
{
    beginStroke();

    for (int i = 0; i < TotalCells; ++i)
    {
        const auto previous = pixels[static_cast<size_t>(i)];
        if (previous == PixelColor::Black)
            continue;

        m_activeStroke.push_back({ static_cast<uint16_t>(i), previous, PixelColor::Black });
        pixels[static_cast<size_t>(i)] = PixelColor::Black;
        m_gridCache[static_cast<size_t>(i)] = static_cast<uint8_t>(PixelColor::Black);
    }

    if (!m_activeStroke.empty())
    {
        m_changedCellCount = 0;
        rebuildPixelImage();
    }

    commitStroke(true);
    repaint();
}

bool PixelCanvasComponent::undo()
{
    if (m_undoStack.empty())
        return false;

    auto transaction = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    for (auto it = transaction.rbegin(); it != transaction.rend(); ++it)
        applyPixelValue(static_cast<int>(it->index), it->previous);

    if (transaction.size() > 512)
        repaint();
    else
        for (const auto& change : transaction)
            repaint(cellBoundsForIndex(static_cast<int>(change.index)).expanded(1));

    notifySnapshot();
    return true;
}

void PixelCanvasComponent::setGridData(const std::array<uint8_t, TotalCells>& data)
{
    for (size_t i = 0; i < data.size(); ++i)
        pixels[i] = pixelFromRaw(data[i]);

    rebuildGridCache();
    rebuildPixelImage();
    m_undoStack.clear();
    m_activeStroke.clear();
    m_activeStrokeOpen = false;
    repaint();
}

void PixelCanvasComponent::rebuildGridCache()
{
    m_changedCellCount = 0;
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        m_gridCache[i] = static_cast<uint8_t>(pixels[i]);
        if (pixels[i] != PixelColor::Black)
            ++m_changedCellCount;
    }
}

void PixelCanvasComponent::rebuildPixelImage()
{
    m_pixelImage = juce::Image(juce::Image::RGB, GridSize, GridSize, false);
    juce::Image::BitmapData bitmap(m_pixelImage, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < GridSize; ++y)
    {
        for (int x = 0; x < GridSize; ++x)
        {
            const auto color = pixels[static_cast<size_t>(y * GridSize + x)];
            bitmap.setPixelColour(x, y, m_theme.canvasPixelColour(static_cast<uint8_t>(color)));
        }
    }
}

void PixelCanvasComponent::updatePixelImage(int index)
{
    if (!m_pixelImage.isValid())
        rebuildPixelImage();

    const int x = index % GridSize;
    const int y = index / GridSize;
    juce::Image::BitmapData bitmap(m_pixelImage, x, y, 1, 1, juce::Image::BitmapData::writeOnly);
    bitmap.setPixelColour(0, 0, m_theme.canvasPixelColour(static_cast<uint8_t>(pixels[static_cast<size_t>(index)])));
}

void PixelCanvasComponent::notifySnapshot()
{
    if (m_onCanvasSnapshot)
        m_onCanvasSnapshot(m_gridCache);
}
