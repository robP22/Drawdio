#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "PedalStructures.h"
#include "IThemeProvider.h"
#include "ResourceManager.h"

class PixelCanvasComponent : public juce::Component
{
public:
    static constexpr int MaxUndoLevels = 32;
    static constexpr float CanvasScaleRatio = 0.90f;

    enum class PixelColor : uint8_t
    {
        Transparent = 5,
        Black = 0,
        White = 4,
        Red = 3,
        Green = 2,
        Blue = 1,
        Yellow = 6,
        Brown = 7,
        Purple = 8,
        Grey = 9,
        Pink = 10,

        BLACK = Black,
        WHITE = White,
        RED = Red,
        GREEN = Green,
        BLUE = Blue
    };

    struct PixelChange
    {
        uint16_t index;
        PixelColor previous;
        PixelColor current;
    };

    using PixelArray = std::array<PixelColor, TotalCells>;
    using CanvasSnapshotCallback = std::function<void(const std::array<uint8_t, TotalCells>&)>;
    using CanvasPenCallback = std::function<void()>;

    explicit PixelCanvasComponent(const ResourceManager& resources, const IThemeProvider& theme);
    ~PixelCanvasComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void clearCanvas();
    bool undo();

    const std::array<uint8_t, TotalCells>& getGridData() const { return m_gridCache; }
    void setGridData(const std::array<uint8_t, TotalCells>& data);

    void setCurrentColor(PixelColor color) { m_currentColor = color; }
    PixelColor getCurrentColor() const { return m_currentColor; }

    void setFillMode(bool active);
    bool isFillMode() const { return m_fillMode; }

    void setBrushRadius(float radius) { m_brushRadius = radius; rebuildBrushTip(); }
    void setCanvasTopOffset(int px) { m_canvasTopOffset = px; repaint(); }

    int getChangedCellCount() const { return m_changedCellCount; }

    void setOnCanvasSnapshot(CanvasSnapshotCallback cb) { m_onCanvasSnapshot = std::move(cb); }
    void setOnPenDown(CanvasPenCallback cb) { m_onPenDown = std::move(cb); }
    void setOnPenUp(CanvasPenCallback cb) { m_onPenUp = std::move(cb); }
    void setOnUndo(CanvasPenCallback cb) { m_onUndo = std::move(cb); }
    void setOnFillModeChanged(std::function<void(bool)> cb) { m_onFillModeChanged = std::move(cb); }

private:
    juce::Point<int> gridCoordsFromUI(int uiX, int uiY) const;
    juce::Rectangle<int> cellBoundsForIndex(int index) const;

    void beginStroke();
    void commitStroke(bool shouldNotify);
    void rasterizeBrushStroke(juce::Point<int> from, juce::Point<int> to);
    void setPixel(int gridX, int gridY, PixelColor color);
    void applyPixelValue(int index, PixelColor color);
    void stampBrushTip(float gridX, float gridY);
    void rebuildBrushTip();

    void rebuildGridCache();
    void rebuildOverlay();
    void updateOverlayPixel(int index);
    void notifySnapshot();
    void floodFill(int startX, int startY);

    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
    PixelArray pixels;
    std::array<uint8_t, TotalCells> m_gridCache;

    CanvasSnapshotCallback m_onCanvasSnapshot;
    CanvasPenCallback m_onPenDown;
    CanvasPenCallback m_onPenUp;
    CanvasPenCallback m_onUndo;
    std::function<void(bool)> m_onFillModeChanged;

    bool m_drawing = false;
    bool m_activeStrokeOpen = false;
    bool m_fillMode = false;
    juce::Point<int> m_lastDrawPos;
    PixelColor m_currentColor = PixelColor::Red;
    float m_brushRadius = 0.75f;
    bool m_brushPainting = false;
    int m_canvasTopOffset = 0;

    std::vector<PixelChange> m_activeStroke;
    std::vector<std::vector<PixelChange>> m_undoStack;
    std::vector<int> m_fillQueue;
    std::vector<std::uint8_t> m_fillVisited;
    std::unordered_map<int, int> m_activeChangeLookup;

    juce::Image m_pixelOverlay;
    juce::Image m_brushTipImage;
    bool m_overlayDirty = true;
    int m_changedCellCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PixelCanvasComponent)
};
