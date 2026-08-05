#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "Core/DrawdioConstants.h"
#include "UI/Theme/IThemeProvider.h"
#include "Core/Contracts/IResourceProvider.h"

class PixelCanvasComponent : public juce::Component
{
public:
    static constexpr int MaxUndoLevels = 32;
    static constexpr size_t MaxUndoBytes = 8u * 1024u * 1024u; // 8 MB across undo+redo

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
        Orange = 11,
        Violet = 12,
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

    explicit PixelCanvasComponent(const IResourceProvider& resources, const IThemeProvider& theme);
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
    bool redo();

    std::vector<uint8_t> captureUndoData() const;
    void applyUndoData(const std::vector<uint8_t>& data);

    const std::array<uint8_t, TotalCells>& getGridData() const { return m_gridCache; }
    void setGridData(const std::array<uint8_t, TotalCells>& data);

    void setCurrentColor(PixelColor color) { m_currentColor = color; }

    void setFillMode(bool active);

    void setBrushRadius(float radius) { m_brushRadius = radius; }
    void setCanvasTopOffset(int px) { m_canvasTopOffset = px; repaint(); }

    void setOnCanvasSnapshot(CanvasSnapshotCallback cb) { m_onCanvasSnapshot = std::move(cb); }
    void setOnPenDown(CanvasPenCallback cb) { m_onPenDown = std::move(cb); }
    void setOnPenUp(CanvasPenCallback cb) { m_onPenUp = std::move(cb); }
    void setOnColorChanged(std::function<void(PixelColor)> cb) { m_onColorChanged = std::move(cb); }

    void setPartyModeEnabled(bool on);

private:
    struct CanvasLayout
    {
        float canvasW = 0.0f;
        float canvasH = 0.0f;
        int cw = 0;
        int ch = 0;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float cellW = 0.0f;
        float cellH = 0.0f;
    };

    CanvasLayout computeCanvasLayout() const;
    juce::Point<int> gridCoordsFromUI(int uiX, int uiY) const;
    juce::Rectangle<int> cellBoundsForIndex(int index) const;

    void beginStroke();
    bool commitStroke();
    void rasterizeBrushStroke(juce::Point<int> from, juce::Point<int> to);
    void setPixel(int gridX, int gridY, PixelColor color);
    void applyPixelValue(int index, PixelColor color, bool doOverlay = true);

    void rebuildGridCache();
    void rebuildOverlay();
    void rebuildOverlay(const CanvasLayout& cl);
    void updateOverlayPixel(int index);
    void flushPendingOverlay();
    void notifySnapshot();
    void floodFill(int startX, int startY);

    PixelColor randomPartyColor();

    const IResourceProvider& m_resources;
    const IThemeProvider& m_theme;
    PixelArray pixels;
    std::array<uint8_t, TotalCells> m_gridCache;

    CanvasSnapshotCallback m_onCanvasSnapshot;
    CanvasPenCallback m_onPenDown;
    CanvasPenCallback m_onPenUp;

    bool m_drawing = false;
    bool m_activeStrokeOpen = false;
    bool m_fillMode = false;
    juce::Point<int> m_lastDrawPos;
    PixelColor m_currentColor = PixelColor::Red;
    float m_brushRadius = 0.75f;
    int m_canvasTopOffset = 0;

    bool m_partyModeEnabled = false;
    mutable bool m_bounceActive = false;
    mutable bool m_justBounced = false;
    std::function<void(PixelColor)> m_onColorChanged;

    std::vector<PixelChange> m_activeStroke;
    std::vector<std::vector<PixelChange>> m_undoStack;
    std::vector<std::vector<PixelChange>> m_redoStack;
    size_t m_undoBytes = 0;
    std::vector<int> m_fillQueue;
    std::vector<std::uint8_t> m_fillVisited;
    std::vector<PixelChange> m_fillChanges;
    juce::Image m_pixelOverlay;
    juce::Image m_grainOverlay;
    bool m_overlayDirty = true;
    int m_changedCellCount = 0;
    std::vector<int> m_pendingOverlayIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PixelCanvasComponent)
};
