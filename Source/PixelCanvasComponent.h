#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "PedalStructures.h"
#include "IThemeProvider.h"

class PixelCanvasComponent : public juce::Component
{
public:
    static constexpr int MaxUndoLevels = 32;
    static constexpr float CanvasScaleRatio = 0.80f;  ///< Scale factor for pixel canvas (80% of component)

    enum class PixelColor : uint8_t
    {
        Black = 0,
        White = 4,
        Red = 3,
        Green = 2,
        Blue = 1,

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

    explicit PixelCanvasComponent(const IThemeProvider& theme);
    ~PixelCanvasComponent() override = default;

    void paint(juce::Graphics& g) override;

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

    int getChangedCellCount() const { return m_changedCellCount; }

    void setOnCanvasSnapshot(CanvasSnapshotCallback cb) { m_onCanvasSnapshot = std::move(cb); }
    void setOnPenDown(CanvasPenCallback cb) { m_onPenDown = std::move(cb); }
    void setOnPenUp(CanvasPenCallback cb) { m_onPenUp = std::move(cb); }

    static juce::Colour colourForPixel(PixelColor color);

private:
    juce::Point<int> gridCoordsFromUI(int uiX, int uiY) const;
    juce::Rectangle<int> cellBoundsForIndex(int index) const;
    void drawCanvasShadow(juce::Graphics& g, const juce::Rectangle<float>& bounds);

    void beginStroke();
    void commitStroke(bool shouldNotify);
    void rasterizeLine(juce::Point<int> from, juce::Point<int> to);
    void setPixel(int gridX, int gridY, PixelColor color);
    void applyPixelValue(int index, PixelColor color);

    void rebuildGridCache();
    void rebuildPixelImage();
    void updatePixelImage(int index);
    void notifySnapshot();

    const IThemeProvider& m_theme;
    PixelArray pixels;
    std::array<uint8_t, TotalCells> m_gridCache;

    CanvasSnapshotCallback m_onCanvasSnapshot;
    CanvasPenCallback m_onPenDown;
    CanvasPenCallback m_onPenUp;

    bool m_drawing = false;
    bool m_mouseInside = false;
    bool m_activeStrokeOpen = false;
    juce::Point<int> m_lastDrawPos;
    PixelColor m_currentColor = PixelColor::Red;

    std::vector<PixelChange> m_activeStroke;
    std::vector<std::vector<PixelChange>> m_undoStack;
    std::array<int, TotalCells> m_activeChangeLookup;

    juce::Image m_pixelImage;
    int m_changedCellCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PixelCanvasComponent)
};
