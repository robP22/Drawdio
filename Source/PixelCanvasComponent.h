#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cstdint>
#include <functional>
#include "PedalStructures.h"

class PixelCanvasComponent : public juce::Component
{
public:
    static constexpr int MaxUndoLevels = 20;

    enum class PixelColor : uint8_t
    {
        BLACK = 0,
        BLUE = 1,
        GREEN = 2,
        RED = 3,
        WHITE = 4
    };

    using CanvasSnapshotCallback = std::function<void(const std::array<uint8_t, TotalCells>&)>;
    using CanvasPenCallback = std::function<void()>;

    PixelCanvasComponent();
    ~PixelCanvasComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void clearCanvas();

    const std::array<uint8_t, TotalCells>& getGridData() const { return m_grid; }
    void setGridData(const std::array<uint8_t, TotalCells>& data);

    void setCurrentColor(PixelColor c) { m_currentColor = c; }

    void pushUndoState();
    void undo();

    void setOnCanvasSnapshot(CanvasSnapshotCallback cb) { m_onCanvasSnapshot = cb; }
    void setOnPenDown(CanvasPenCallback cb) { m_onPenDown = cb; }
    void setOnPenUp(CanvasPenCallback cb) { m_onPenUp = cb; }

private:
    juce::Point<int> gridCoordsFromUI(int uiX, int uiY) const;
    void rasterizeLine(juce::Point<int> from, juce::Point<int> to);
    void setPixel(int gridX, int gridY, PixelColor color);

    std::array<uint8_t, TotalCells> m_grid;
    CanvasSnapshotCallback m_onCanvasSnapshot;
    CanvasPenCallback m_onPenDown;
    CanvasPenCallback m_onPenUp;
    bool m_drawing;
    juce::Point<int> m_lastDrawPos;
    PixelColor m_currentColor;

    std::vector<std::array<uint8_t, TotalCells>> m_undoStack;
    int m_undoIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PixelCanvasComponent)
};
