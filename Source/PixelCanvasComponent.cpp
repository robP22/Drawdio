#include "PixelCanvasComponent.h"
#include "GridLayout.h"
#include <cmath>
#include <utility>

namespace
{
static uint8_t pixelToGridValue(PixelCanvasComponent::PixelColor c)
{
    return (c == PixelCanvasComponent::PixelColor::Transparent) ? 0
         : (c == PixelCanvasComponent::PixelColor::Black) ? 5
         : static_cast<uint8_t>(c);
}

static PixelCanvasComponent::PixelColor gridValueToPixel(uint8_t raw)
{
    return (raw == 0) ? PixelCanvasComponent::PixelColor::Transparent
         : (raw == 5) ? PixelCanvasComponent::PixelColor::Black
         : static_cast<PixelCanvasComponent::PixelColor>(raw);
}
}

PixelCanvasComponent::PixelCanvasComponent(const ResourceManager& resources, const IThemeProvider& theme)
    : m_resources(resources), m_theme(theme)
{
    pixels.fill(PixelColor::Transparent);
    m_activeStroke.reserve(512);
    m_undoStack.reserve(MaxUndoLevels);
    m_fillChanges.reserve(TotalCells);

    rebuildGridCache();
}

void PixelCanvasComponent::resized()
{
    m_overlayDirty = true;
    repaint();

    constexpr int gs = 256;
    m_grainOverlay = juce::Image(juce::Image::ARGB, gs, gs, true);
    if (m_grainOverlay.isValid())
    {
        juce::Graphics grainG(m_grainOverlay);
        auto& rng = juce::Random::getSystemRandom();
        for (int y = 0; y < gs; y += 4)
            for (int x = 0; x < gs; x += 4)
            {
                grainG.setColour(juce::Colour(static_cast<uint8_t>(255), static_cast<uint8_t>(255), static_cast<uint8_t>(255), static_cast<uint8_t>(rng.nextInt(5))));
                grainG.fillRect(static_cast<float>(x), static_cast<float>(y), 4.0f, 4.0f);
            }
    }
}

PixelCanvasComponent::CanvasLayout PixelCanvasComponent::computeCanvasLayout() const
{
    CanvasLayout cl;
    cl.canvasW = getWidth() * GridLayout::CanvasScaleRatio;
    cl.canvasH = getHeight() * GridLayout::CanvasScaleRatio;
    cl.cw = juce::jmax(1, static_cast<int>(cl.canvasW));
    cl.ch = juce::jmax(1, static_cast<int>(cl.canvasH));
    cl.offsetX = (getWidth() - cl.canvasW) / 2.0f + getWidth() * GridLayout::CanvasCenterXShiftRatio;
    cl.offsetY = (m_canvasTopOffset > 0)
        ? static_cast<float>(m_canvasTopOffset)
        : (getHeight() - cl.canvasH) / 2.0f + getHeight() * GridLayout::CanvasCenterYShiftRatio;
    cl.cellW = cl.canvasW / GridSize;
    cl.cellH = cl.canvasH / GridSize;
    return cl;
}

void PixelCanvasComponent::paint(juce::Graphics& g)
{
    auto cl = computeCanvasLayout();
    const int cx = static_cast<int>(cl.offsetX);
    const int cy = static_cast<int>(cl.offsetY);
    const int cw = cl.cw;
    const int ch = cl.ch;

    const auto& tex = m_resources.getTexture(ResourceManager::TextureId::CanvasTexture);
    if (tex.isValid())
        g.drawImage(tex, cx, cy, cw, ch, 0, 0, tex.getWidth(), tex.getHeight());

    if (m_overlayDirty || !m_pixelOverlay.isValid())
        rebuildOverlay();

    if (m_pixelOverlay.isValid())
    {
        g.setOpacity(0.88f);
        g.drawImageAt(m_pixelOverlay, cx, cy);
        g.setOpacity(1.0f);
    }

    if (m_grainOverlay.isValid())
        g.drawImage(m_grainOverlay, cx, cy, cw, ch,
                    0, 0, m_grainOverlay.getWidth(), m_grainOverlay.getHeight());
}

juce::Point<int> PixelCanvasComponent::gridCoordsFromUI(int uiX, int uiY) const
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return {};

    auto cl = computeCanvasLayout();

    float nx = (static_cast<float>(uiX) - cl.offsetX) / cl.canvasW;
    float ny = (static_cast<float>(uiY) - cl.offsetY) / cl.canvasH;

    if (m_partyModeEnabled)
    {
        bool outside = (nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f);
        if (outside && !m_bounceActive) { m_bounceActive = true; m_justBounced = true; }
        else if (!outside && m_bounceActive) { m_bounceActive = false; }

        auto fold = [](float v) -> float {
            v = std::fmod(std::abs(v), 2.0f);
            return (v > 1.0f) ? 2.0f - v : v;
        };

        nx = fold(nx);
        ny = fold(ny);
    }

    int gridX = juce::jlimit(0, GridSize - 1,
        static_cast<int>(nx * static_cast<float>(GridSize - 1)));
    int gridY = juce::jlimit(0, GridSize - 1,
        static_cast<int>(ny * static_cast<float>(GridSize - 1)));
    return { gridX, gridY };
}

juce::Rectangle<int> PixelCanvasComponent::cellBoundsForIndex(int index) const
{
    if (getWidth() == 0)
        return {};

    const int x = index % GridSize;
    const int y = index / GridSize;

    auto cl = computeCanvasLayout();

    return juce::Rectangle<float>(cl.offsetX + static_cast<float>(x) * cl.cellW,
                                   cl.offsetY + static_cast<float>(y) * cl.cellH,
                                   cl.cellW * GridLayout::CellOverdrawRatio,
                                   cl.cellH * GridLayout::CellOverdrawRatio).getSmallestIntegerContainer();
}

void PixelCanvasComponent::beginStroke()
{
    if (m_activeStrokeOpen)
        return;

    m_activeStrokeOpen = true;
    m_activeStroke.clear();
    m_redoStack.clear();
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

void PixelCanvasComponent::rasterizeBrushStroke(juce::Point<int> from, juce::Point<int> to)
{
    float dx = static_cast<float>(to.x - from.x);
    float dy = static_cast<float>(to.y - from.y);
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 0.001f)
    {
        int minX = std::max(0, static_cast<int>(static_cast<float>(from.x) - m_brushRadius));
        int maxX = std::min(GridSize - 1, static_cast<int>(static_cast<float>(from.x) + m_brushRadius));
        int minY = std::max(0, static_cast<int>(static_cast<float>(from.y) - m_brushRadius));
        int maxY = std::min(GridSize - 1, static_cast<int>(static_cast<float>(from.y) + m_brushRadius));
        for (int gy = minY; gy <= maxY; ++gy)
            for (int gx = minX; gx <= maxX; ++gx)
            {
                float gcx = static_cast<float>(gx) + 0.5f;
                float gcy = static_cast<float>(gy) + 0.5f;
                float d2 = (gcx - static_cast<float>(from.x)) * (gcx - static_cast<float>(from.x))
                         + (gcy - static_cast<float>(from.y)) * (gcy - static_cast<float>(from.y));
                if (d2 <= m_brushRadius * m_brushRadius + 0.25f)
                    setPixel(gx, gy, m_currentColor);
            }
        return;
    }

    float stepSize = 0.3f;
    int steps = std::max(1, static_cast<int>(dist / stepSize));
    float radiusSq = m_brushRadius * m_brushRadius + 0.25f;

    for (int i = 0; i <= steps; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        float cx = static_cast<float>(from.x) + dx * t;
        float cy = static_cast<float>(from.y) + dy * t;

        int minX = std::max(0, static_cast<int>(cx - m_brushRadius));
        int maxX = std::min(GridSize - 1, static_cast<int>(cx + m_brushRadius));
        int minY = std::max(0, static_cast<int>(cy - m_brushRadius));
        int maxY = std::min(GridSize - 1, static_cast<int>(cy + m_brushRadius));

        for (int gy = minY; gy <= maxY; ++gy)
            for (int gx = minX; gx <= maxX; ++gx)
            {
                float gcx = static_cast<float>(gx) + 0.5f;
                float gcy = static_cast<float>(gy) + 0.5f;
                float d2 = (gcx - cx) * (gcx - cx) + (gcy - cy) * (gcy - cy);
                if (d2 <= radiusSq)
                    setPixel(gx, gy, m_currentColor);
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
        m_activeStroke.push_back({ static_cast<uint16_t>(index), previous, color });

    applyPixelValue(index, color);
    repaint(cellBoundsForIndex(index).expanded(1));
}

void PixelCanvasComponent::applyPixelValue(int index, PixelColor color, bool doOverlay)
{
    const auto previous = pixels[static_cast<size_t>(index)];
    if (previous == PixelColor::Transparent && color != PixelColor::Transparent)
        ++m_changedCellCount;
    else if (previous != PixelColor::Transparent && color == PixelColor::Transparent)
        --m_changedCellCount;

    pixels[static_cast<size_t>(index)] = color;
    m_gridCache[static_cast<size_t>(index)] = pixelToGridValue(color);
    if (doOverlay)
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
    rasterizeBrushStroke(pos, pos);
    m_lastDrawPos = pos;
}

void PixelCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_drawing)
        return;

    auto pos = gridCoordsFromUI(event.x, event.y);

    if (m_justBounced)
    {
        auto newColor = randomPartyColor();
        m_currentColor = newColor;
        if (m_onColorChanged)
            m_onColorChanged(newColor);
        m_justBounced = false;
    }

    rasterizeBrushStroke(m_lastDrawPos, pos);
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
    m_redoStack.push_back(std::move(transaction));
    auto& t = m_redoStack.back();

    for (auto it = t.rbegin(); it != t.rend(); ++it)
        applyPixelValue(static_cast<int>(it->index), it->previous, false);
    m_overlayDirty = true;

    if (t.size() > 512)
        repaint();
    else
        for (const auto& change : t)
            repaint(cellBoundsForIndex(static_cast<int>(change.index)).expanded(1));

    notifySnapshot();
    if (m_onUndo)
        m_onUndo();
    return true;
}

bool PixelCanvasComponent::redo()
{
    if (m_redoStack.empty())
        return false;

    auto transaction = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    m_undoStack.push_back(std::move(transaction));
    auto& t = m_undoStack.back();

    for (auto it = t.begin(); it != t.end(); ++it)
        applyPixelValue(static_cast<int>(it->index), it->current, false);
    m_overlayDirty = true;

    if (t.size() > 512)
        repaint();
    else
        for (const auto& change : t)
            repaint(cellBoundsForIndex(static_cast<int>(change.index)).expanded(1));

    notifySnapshot();
    return true;
}

std::vector<uint8_t> PixelCanvasComponent::captureUndoData() const
{
    if (m_undoStack.empty()) return {};

    std::vector<uint8_t> data;
    auto writeU32 = [&](uint32_t v) {
        data.push_back(static_cast<uint8_t>(v));
        data.push_back(static_cast<uint8_t>(v >> 8));
        data.push_back(static_cast<uint8_t>(v >> 16));
        data.push_back(static_cast<uint8_t>(v >> 24));
    };

    writeU32(static_cast<uint32_t>(m_undoStack.size()));
    for (auto& transaction : m_undoStack)
    {
        writeU32(static_cast<uint32_t>(transaction.size()));
        for (auto& change : transaction)
        {
            data.push_back(static_cast<uint8_t>(change.index));
            data.push_back(static_cast<uint8_t>(change.index >> 8));
            data.push_back(static_cast<uint8_t>(change.previous));
            data.push_back(static_cast<uint8_t>(change.current));
        }
    }
    return data;
}

void PixelCanvasComponent::applyUndoData(const std::vector<uint8_t>& data)
{
    m_undoStack.clear();
    m_activeStroke.clear();
    m_activeStrokeOpen = false;

    if (data.empty()) return;

    size_t pos = 0;
    auto readU32 = [&]() -> uint32_t {
        if (pos + 4 > data.size()) return 0;
        uint32_t v = data[pos] | (static_cast<uint32_t>(data[pos + 1]) << 8)
             | (static_cast<uint32_t>(data[pos + 2]) << 16)
             | (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return v;
    };

    uint32_t numTransactions = readU32();
    for (uint32_t t = 0; t < numTransactions; ++t)
    {
        uint32_t numChanges = readU32();
        std::vector<PixelChange> transaction;
        transaction.reserve(numChanges);

        for (uint32_t c = 0; c < numChanges; ++c)
        {
            if (pos + 4 > data.size()) return;
            uint16_t index = static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
            pos += 2;
            auto previous = static_cast<PixelColor>(data[pos++]);
            auto current  = static_cast<PixelColor>(data[pos++]);
            transaction.push_back({ index, previous, current });
        }
        m_undoStack.push_back(std::move(transaction));
    }
}

void PixelCanvasComponent::setGridData(const std::array<uint8_t, TotalCells>& data)
{
    for (size_t i = 0; i < data.size(); ++i)
        pixels[i] = gridValueToPixel(data[i]);

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
        m_gridCache[i] = pixelToGridValue(pixels[i]);
        if (pixels[i] != PixelColor::Transparent)
            ++m_changedCellCount;
    }
}

void PixelCanvasComponent::rebuildOverlay()
{
    auto cl = computeCanvasLayout();
    const int cw = cl.cw;
    const int ch = cl.ch;

    if (cw < 2 || ch < 2)
    {
        m_overlayDirty = true;
        return;
    }

    m_pixelOverlay = juce::Image(juce::Image::ARGB, cw, ch, true);
    if (!m_pixelOverlay.isValid())
        return;

    const float cellW = cl.cellW;
    const float cellH = cl.cellH;

    {
        juce::Image::BitmapData bd(m_pixelOverlay, juce::Image::BitmapData::writeOnly);

        for (size_t i = 0; i < pixels.size(); ++i)
        {
            if (pixels[i] == PixelColor::Transparent)
                continue;

            const int x = static_cast<int>(i) % GridSize;
            const int y = static_cast<int>(i) / GridSize;
            const int px = static_cast<int>(std::round(static_cast<float>(x) * cellW));
            const int py = static_cast<int>(std::round(static_cast<float>(y) * cellH));
            const int pw = std::max(1, static_cast<int>(std::round(static_cast<float>(x + 1) * cellW)) - px);
            const int ph = std::max(1, static_cast<int>(std::round(static_cast<float>(y + 1) * cellH)) - py);

            auto col = m_theme.canvasPixelColour(static_cast<uint8_t>(pixels[i]));

            for (int sy = 0; sy < ph; ++sy)
            {
                auto* row = reinterpret_cast<juce::PixelARGB*>(bd.getPixelPointer(px, py + sy));
                for (int sx = 0; sx < pw; ++sx)
                    row[sx].setARGB(col.getAlpha(), col.getRed(), col.getGreen(), col.getBlue());
            }
        }
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

    auto cl = computeCanvasLayout();
    const float cellW = cl.cellW;
    const float cellH = cl.cellH;
    const int x = index % GridSize;
    const int y = index / GridSize;

    const int px = static_cast<int>(std::round(static_cast<float>(x) * cellW));
    const int py = static_cast<int>(std::round(static_cast<float>(y) * cellH));
    const int pw = std::max(1, static_cast<int>(std::round(static_cast<float>(x + 1) * cellW)) - px);
    const int ph = std::max(1, static_cast<int>(std::round(static_cast<float>(y + 1) * cellH)) - py);

    auto col = (pixels[static_cast<size_t>(index)] != PixelColor::Transparent)
        ? m_theme.canvasPixelColour(static_cast<uint8_t>(pixels[static_cast<size_t>(index)]))
        : juce::Colours::transparentBlack;

    {
        juce::Image::BitmapData bd(m_pixelOverlay, juce::Image::BitmapData::writeOnly);

        for (int sy = 0; sy < ph; ++sy)
        {
            auto* row = reinterpret_cast<juce::PixelARGB*>(bd.getPixelPointer(px, py + sy));
            for (int sx = 0; sx < pw; ++sx)
                row[sx].setARGB(col.getAlpha(), col.getRed(), col.getGreen(), col.getBlue());
        }
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

void PixelCanvasComponent::setPartyModeEnabled(bool on)
{
    m_partyModeEnabled = on;
    m_bounceActive = false;
    m_justBounced = false;
}

PixelCanvasComponent::PixelColor PixelCanvasComponent::randomPartyColor()
{
    static constexpr PixelColor colors[] = {
        PixelColor::Black, PixelColor::Brown, PixelColor::Purple,
        PixelColor::Blue, PixelColor::Green, PixelColor::Grey,
        PixelColor::Pink, PixelColor::Yellow, PixelColor::Red, PixelColor::White
    };
    return colors[juce::Random::getSystemRandom().nextInt(10)];
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

    m_fillChanges.clear();

    static constexpr int dx[4] = {-1, 1, 0, 0};
    static constexpr int dy[4] = {0, 0, -1, 1};

    size_t head = 0;
    while (head < m_fillQueue.size())
    {
        int idx = m_fillQueue[head++];
        m_fillChanges.push_back({static_cast<uint16_t>(idx), targetColor, fillColor});

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

    if (m_fillChanges.empty())
        return;

    for (const auto& ch : m_fillChanges)
    {
        size_t i = static_cast<size_t>(ch.index);
        pixels[i] = fillColor;
        m_gridCache[i] = pixelToGridValue(fillColor);
    }

    m_undoStack.push_back({});
    m_undoStack.back().swap(m_fillChanges);
    if (m_undoStack.size() > static_cast<size_t>(MaxUndoLevels))
        m_undoStack.erase(m_undoStack.begin());

    // Schedule overlay rebuild — BFS already updated pixels array
    m_overlayDirty = true;

    repaint();
    notifySnapshot();
}
