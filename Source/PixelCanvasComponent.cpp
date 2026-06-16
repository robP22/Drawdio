#include "PixelCanvasComponent.h"
#include "GridLayout.h"
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
        case 5: return PixelCanvasComponent::PixelColor::Black;
        case 6: return PixelCanvasComponent::PixelColor::Yellow;
        case 7: return PixelCanvasComponent::PixelColor::Brown;
        case 8: return PixelCanvasComponent::PixelColor::Purple;
        case 9: return PixelCanvasComponent::PixelColor::Grey;
        case 10: return PixelCanvasComponent::PixelColor::Pink;
        case 0: return PixelCanvasComponent::PixelColor::Transparent;
        default: return PixelCanvasComponent::PixelColor::Transparent;
    }
}
}

PixelCanvasComponent::PixelCanvasComponent(const ResourceManager& resources, const IThemeProvider& theme)
    : m_resources(resources), m_theme(theme)
{
    pixels.fill(PixelColor::Transparent);
    m_activeStroke.reserve(512);
    m_undoStack.reserve(MaxUndoLevels);

    rebuildGridCache();
}

void PixelCanvasComponent::resized()
{
    m_overlayDirty = true;
    repaint();
}

void PixelCanvasComponent::paint(juce::Graphics& g)
{
    const float canvasW = getWidth() * CanvasScaleRatio;
    const float canvasH = getHeight() * CanvasScaleRatio;
    const int cx = static_cast<int>((getWidth() - canvasW) / 2.0f + getWidth() * GridLayout::CanvasCenterXShiftRatio);
    const int cy = static_cast<int>((getHeight() - canvasH) / 2.0f + getHeight() * GridLayout::CanvasCenterYShiftRatio);
    const int cw = juce::jmax(1, static_cast<int>(canvasW));
    const int ch = juce::jmax(1, static_cast<int>(canvasH));

    const auto& tex = m_resources.getTexture(ResourceManager::TextureId::CanvasTexture);
    if (tex.isValid())
        g.drawImage(tex, cx, cy, cw, ch, 0, 0, tex.getWidth(), tex.getHeight());

    if (m_overlayDirty || !m_pixelOverlay.isValid())
        rebuildOverlay();

    if (m_pixelOverlay.isValid())
        g.drawImageAt(m_pixelOverlay, cx, cy);
}

juce::Point<int> PixelCanvasComponent::gridCoordsFromUI(int uiX, int uiY) const
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return {};

    const float canvasW = bounds.getWidth() * CanvasScaleRatio;
    const float canvasH = bounds.getHeight() * CanvasScaleRatio;
    const float offsetX = (bounds.getWidth() - canvasW) / 2.0f + bounds.getWidth() * GridLayout::CanvasCenterXShiftRatio;
    const float offsetY = (bounds.getHeight() - canvasH) / 2.0f + bounds.getHeight() * GridLayout::CanvasCenterYShiftRatio;

    const int relX = static_cast<int>(static_cast<float>(uiX) - offsetX);
    const int relY = static_cast<int>(static_cast<float>(uiY) - offsetY);

    const int gridX = (relX * GridSize) / juce::jmax(1, static_cast<int>(canvasW));
    const int gridY = (relY * GridSize) / juce::jmax(1, static_cast<int>(canvasH));
    return { juce::jlimit(0, GridSize - 1, gridX),
             juce::jlimit(0, GridSize - 1, gridY) };
}

juce::Rectangle<int> PixelCanvasComponent::cellBoundsForIndex(int index) const
{
    if (getWidth() == 0)
        return {};

    const int x = index % GridSize;
    const int y = index / GridSize;

    const float canvasW = getWidth() * CanvasScaleRatio;
    const float canvasH = getHeight() * CanvasScaleRatio;
    const float offsetX = (getWidth() - canvasW) / 2.0f + getWidth() * GridLayout::CanvasCenterXShiftRatio;
    const float offsetY = (getHeight() - canvasH) / 2.0f + getHeight() * GridLayout::CanvasCenterYShiftRatio;

    const float cellW = canvasW / GridSize;
    const float cellH = canvasH / GridSize;

    return juce::Rectangle<float>(offsetX + static_cast<float>(x) * cellW,
                                   offsetY + static_cast<float>(y) * cellH,
                                   cellW * GridLayout::CellOverdrawRatio,
                                   cellH * GridLayout::CellOverdrawRatio).getSmallestIntegerContainer();
}

void PixelCanvasComponent::beginStroke()
{
    if (m_activeStrokeOpen)
        return;

    m_activeStrokeOpen = true;
    m_activeStroke.clear();
    m_activeChangeLookup.clear();
}

void PixelCanvasComponent::commitStroke(bool)
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
        auto it = m_activeChangeLookup.find(index);
        if (it == m_activeChangeLookup.end())
        {
            m_activeChangeLookup[index] = static_cast<int>(m_activeStroke.size());
            m_activeStroke.push_back({ static_cast<uint16_t>(index), previous, color });
        }
        else
        {
            m_activeStroke[static_cast<size_t>(it->second)].current = color;
        }
    }

    applyPixelValue(index, color);
    repaint(cellBoundsForIndex(index).expanded(1));
}

void PixelCanvasComponent::applyPixelValue(int index, PixelColor color)
{
    const auto previous = pixels[static_cast<size_t>(index)];
    if (previous == PixelColor::Transparent && color != PixelColor::Transparent)
        ++m_changedCellCount;
    else if (previous != PixelColor::Transparent && color == PixelColor::Transparent)
        --m_changedCellCount;

    pixels[static_cast<size_t>(index)] = color;
    m_gridCache[static_cast<size_t>(index)] = (color == PixelColor::Transparent) ? 0 :
        (color == PixelColor::Black) ? 5 : static_cast<uint8_t>(color);
    updateOverlayPixel(index);
}

void PixelCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    if (m_fillMode)
    {
        auto pos = gridCoordsFromUI(event.x, event.y);
        if (pos.x >= 0 && pos.x < GridSize && pos.y >= 0 && pos.y < GridSize)
            floodFill(pos.x, pos.y);
        return;
    }

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
    commitStroke(false);
    notifySnapshot();

    if (m_onPenUp)
        m_onPenUp();
}

void PixelCanvasComponent::mouseEnter(const juce::MouseEvent&)
{
    if (m_fillMode)
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void PixelCanvasComponent::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);

    if (m_activeStrokeOpen)
    {
        m_activeStroke.clear();
        m_activeChangeLookup.clear();
        m_activeStrokeOpen = false;
        m_drawing = false;
    }
}

void PixelCanvasComponent::clearCanvas()
{
    beginStroke();

    for (int i = 0; i < TotalCells; ++i)
    {
        const auto previous = pixels[static_cast<size_t>(i)];
        if (previous == PixelColor::Transparent)
            continue;

        m_activeStroke.push_back({ static_cast<uint16_t>(i), previous, PixelColor::Transparent });
        applyPixelValue(i, PixelColor::Transparent);
    }

    if (!m_activeStroke.empty())
    {
        m_changedCellCount = 0;
        commitStroke(false);
        rebuildOverlay();
        notifySnapshot();
    }
    else
    {
        m_activeStroke.clear();
        m_activeStrokeOpen = false;
    }
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

    rebuildOverlay();

    if (transaction.size() > 512)
        repaint();
    else
        for (const auto& change : transaction)
            repaint(cellBoundsForIndex(static_cast<int>(change.index)).expanded(1));

    notifySnapshot();
    if (m_onUndo)
        m_onUndo();
    return true;
}

void PixelCanvasComponent::setGridData(const std::array<uint8_t, TotalCells>& data)
{
    for (size_t i = 0; i < data.size(); ++i)
        pixels[i] = pixelFromRaw(data[i]);

    rebuildGridCache();
    m_overlayDirty = true;
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
        m_gridCache[i] = (pixels[i] == PixelColor::Transparent) ? 0 :
            (pixels[i] == PixelColor::Black) ? 5 : static_cast<uint8_t>(pixels[i]);
        if (pixels[i] != PixelColor::Transparent)
            ++m_changedCellCount;
    }
}

void PixelCanvasComponent::rebuildOverlay()
{
    const float canvasW = getWidth() * CanvasScaleRatio;
    const float canvasH = getHeight() * CanvasScaleRatio;
    const int cw = juce::jmax(1, static_cast<int>(canvasW));
    const int ch = juce::jmax(1, static_cast<int>(canvasH));

    if (cw < 2 || ch < 2)
    {
        m_overlayDirty = true;
        return;
    }

    m_pixelOverlay = juce::Image(juce::Image::ARGB, cw, ch, true);
    if (!m_pixelOverlay.isValid())
        return;

    juce::Graphics g(m_pixelOverlay);
    const float cellW = canvasW / GridSize;
    const float cellH = canvasH / GridSize;

    for (size_t i = 0; i < pixels.size(); ++i)
    {
        if (pixels[i] == PixelColor::Transparent)
            continue;

        const int x = static_cast<int>(i) % GridSize;
        const int y = static_cast<int>(i) / GridSize;
        g.setColour(m_theme.canvasPixelColour(static_cast<uint8_t>(pixels[i])));
        g.fillRect(static_cast<float>(x) * cellW,
                   static_cast<float>(y) * cellH,
                   cellW + 1.0f, cellH + 1.0f);
    }

    m_overlayDirty = false;
}

void PixelCanvasComponent::updateOverlayPixel(int index)
{
    if (!m_pixelOverlay.isValid())
    {
        m_overlayDirty = true;
        return;
    }

    const float canvasW = getWidth() * CanvasScaleRatio;
    const float canvasH = getHeight() * CanvasScaleRatio;
    const float cellW = canvasW / GridSize;
    const float cellH = canvasH / GridSize;
    const int x = index % GridSize;
    const int y = index / GridSize;

    juce::Graphics g(m_pixelOverlay);
    g.setColour(juce::Colours::transparentBlack);
    g.fillRect(static_cast<float>(x) * cellW,
               static_cast<float>(y) * cellH,
               cellW + 1.0f, cellH + 1.0f);
    if (pixels[static_cast<size_t>(index)] != PixelColor::Transparent)
    {
        g.setColour(m_theme.canvasPixelColour(static_cast<uint8_t>(pixels[static_cast<size_t>(index)])));
        g.fillRect(static_cast<float>(x) * cellW,
                   static_cast<float>(y) * cellH,
                   cellW + 1.0f, cellH + 1.0f);
    }
}

void PixelCanvasComponent::notifySnapshot()
{
    if (m_onCanvasSnapshot)
        m_onCanvasSnapshot(m_gridCache);
}

void PixelCanvasComponent::setFillMode(bool active)
{
    m_fillMode = active;
    setMouseCursor(active ? juce::MouseCursor::CrosshairCursor
                          : juce::MouseCursor::NormalCursor);
    repaint();
}

void PixelCanvasComponent::floodFill(int startX, int startY)
{
    size_t startIdx = static_cast<size_t>(startY * GridSize + startX);
    PixelColor fillColor = m_currentColor;
    PixelColor targetColor = pixels[startIdx];

    if (targetColor == fillColor || fillColor == PixelColor::Transparent)
        return;

    m_fillQueue.clear();
    m_fillQueue.reserve(TotalCells);
    m_fillQueue.push_back(startX + startY * GridSize);

    m_fillVisited.assign(TotalCells, 0);
    m_fillVisited[startIdx] = 1;

    std::vector<PixelChange> fillChanges;
    fillChanges.reserve(TotalCells);

    static constexpr int dx[4] = {-1, 1, 0, 0};
    static constexpr int dy[4] = {0, 0, -1, 1};

    size_t head = 0;
    while (head < m_fillQueue.size())
    {
        int idx = m_fillQueue[head++];
        fillChanges.push_back({static_cast<uint16_t>(idx), targetColor, fillColor});

        int x = idx % GridSize;
        int y = idx / GridSize;

        for (int d = 0; d < 4; ++d)
        {
            int nx = x + dx[d];
            int ny = y + dy[d];
            if (nx < 0 || nx >= GridSize || ny < 0 || ny >= GridSize)
                continue;
            int ni = ny * GridSize + nx;
            size_t ni_s = static_cast<size_t>(ni);
            if (m_fillVisited[ni_s] == 0 && pixels[ni_s] == targetColor)
            {
                m_fillVisited[ni_s] = 1;
                m_fillQueue.push_back(ni);
            }
        }
    }

    if (fillChanges.empty())
        return;

    for (const auto& ch : fillChanges)
    {
        size_t i = static_cast<size_t>(ch.index);
        pixels[i] = fillColor;
        m_gridCache[i] = (fillColor == PixelColor::Black) ? 5 : static_cast<uint8_t>(fillColor);
        ++m_changedCellCount;
    }

    m_undoStack.push_back(std::move(fillChanges));
    if (m_undoStack.size() > static_cast<size_t>(MaxUndoLevels))
        m_undoStack.erase(m_undoStack.begin());

    rebuildOverlay();
    repaint();
    notifySnapshot();
}
