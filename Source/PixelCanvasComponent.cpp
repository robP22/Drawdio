#include "PixelCanvasComponent.h"

PixelCanvasComponent::PixelCanvasComponent()
    : m_drawing(false),
      m_currentColor(PixelColor::WHITE),
      m_undoIndex(-1)
{
    m_grid.fill(0);
    setRepaintsOnMouseActivity(true);
}

PixelCanvasComponent::~PixelCanvasComponent() {}

void PixelCanvasComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);

    auto bounds = getLocalBounds().toFloat();
    float cellW = bounds.getWidth() / static_cast<float>(GridSize);
    float cellH = bounds.getHeight() / static_cast<float>(GridSize);

    for (int y = 0; y < GridSize; ++y)
    {
        for (int x = 0; x < GridSize; ++x)
        {
            uint8_t val = m_grid[y * GridSize + x];
            juce::Colour color;
            switch (val)
            {
                case 0: color = juce::Colours::black; break;
                case 1: color = juce::Colours::blue; break;
                case 2: color = juce::Colours::green; break;
                case 3: color = juce::Colours::red; break;
                case 4: color = juce::Colours::white; break;
                default: color = juce::Colours::black; break;
            }
            g.setColour(color);
            g.fillRect(x * cellW, y * cellH, cellW, cellH);
        }
    }

    g.setColour(juce::Colours::grey);
    for (int i = 0; i <= GridSize; i += 16)
    {
        float pos = i * cellW;
        g.drawLine(pos, 0, pos, bounds.getHeight(), 0.25f);
        g.drawLine(0, pos, bounds.getWidth(), pos, 0.25f);
    }
}

void PixelCanvasComponent::resized() {}

juce::Point<int> PixelCanvasComponent::gridCoordsFromUI(int uiX, int uiY) const
{
    auto bounds = getLocalBounds();
    int gridX = (uiX * GridSize) / bounds.getWidth();
    int gridY = (uiY * GridSize) / bounds.getHeight();
    return { juce::jlimit(0, GridSize - 1, gridX), juce::jlimit(0, GridSize - 1, gridY) };
}

void PixelCanvasComponent::rasterizeLine(juce::Point<int> from, juce::Point<int> to)
{
    int x0 = from.x, y0 = from.y;
    int x1 = to.x, y1 = to.y;
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        setPixel(x0, y0, m_currentColor);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void PixelCanvasComponent::setPixel(int gridX, int gridY, PixelColor color)
{
    if (gridX < 0 || gridX >= GridSize || gridY < 0 || gridY >= GridSize)
        return;

    uint8_t oldVal = m_grid[gridY * GridSize + gridX];
    uint8_t newVal = static_cast<uint8_t>(color);
    if (newVal != oldVal)
        m_grid[gridY * GridSize + gridX] = newVal;
}

void PixelCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    m_drawing = true;
    if (m_onPenDown) m_onPenDown();
    auto pos = gridCoordsFromUI(event.x, event.y);
    m_lastDrawPos = pos;
    setPixel(pos.x, pos.y, m_currentColor);
    repaint();
}

void PixelCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_drawing) return;
    auto pos = gridCoordsFromUI(event.x, event.y);
    rasterizeLine(m_lastDrawPos, pos);
    m_lastDrawPos = pos;
    repaint();
}

void PixelCanvasComponent::mouseUp(const juce::MouseEvent& event)
{
    m_drawing = false;
    if (m_onPenUp) m_onPenUp();
    if (m_onCanvasSnapshot) m_onCanvasSnapshot(m_grid);
}

void PixelCanvasComponent::mouseEnter(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void PixelCanvasComponent::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void PixelCanvasComponent::clearCanvas()
{
    pushUndoState();
    m_grid.fill(0);
    repaint();
}

void PixelCanvasComponent::setGridData(const std::array<uint8_t, TotalCells>& data)
{
    m_grid = data;
    repaint();
}

void PixelCanvasComponent::pushUndoState()
{
    if (m_undoStack.size() > static_cast<size_t>(m_undoIndex + 1))
        m_undoStack.resize(static_cast<size_t>(m_undoIndex + 1));

    m_undoStack.push_back(m_grid);
    m_undoIndex++;

    if (m_undoStack.size() > static_cast<size_t>(MaxUndoLevels))
    {
        m_undoStack.erase(m_undoStack.begin());
        m_undoIndex--;
    }
}

void PixelCanvasComponent::undo()
{
    if (m_undoIndex >= 0)
    {
        m_grid = m_undoStack[static_cast<size_t>(m_undoIndex)];
        m_undoStack.resize(static_cast<size_t>(m_undoIndex));
        m_undoIndex--;
        repaint();
    }
}
