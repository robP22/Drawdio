#pragma once
#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "Core/DrawdioConstants.h"
#include "State/ManualConnectionModel.h"
#include "JackHitMap.h"

class ManualRoutingController
{
public:
    ManualRoutingController(ManualConnectionModel& connectionModel,
                            JackHitMap& jackMap,
                            std::function<bool()> isManualMode);

    void mouseDown(juce::Point<float> pos);
    void mouseDrag(juce::Point<float> pos);
    void mouseUp(juce::Point<float> pos,
                 std::function<void()> commitRouting,
                 std::function<void()> rebuildCables);

    bool isDragging() const { return m_dragMode != DragMode::None; }
    bool isNewCableDrag() const { return m_dragMode == DragMode::NewCable; }
    bool isGrabDrag() const { return m_dragMode == DragMode::GrabCable; }
    int dragSrcJackIdx() const { return m_dragSrcJackIdx; }
    juce::Point<float> dragStartPos() const { return m_dragStartPos; }
    juce::Point<float> dragCurrentPos() const { return m_dragCurrentPos; }
    std::vector<int> validTargetJackIndices() const;

    int grabbedEdgeIndex() const { return m_grabbedEdgeIndex; }
    bool grabbingSrcEnd() const { return m_grabbingSrcEnd; }
    int grabbedSrcSlot() const { return m_grabbedSrcSlot; }
    int grabbedDstSlot() const { return m_grabbedDstSlot; }
    juce::Point<float> anchoredPos() const { return m_anchoredPos; }
    void setAnchoredPos(juce::Point<float> pos) { m_anchoredPos = pos; }

private:
    enum class DragMode { None, NewCable, GrabCable };

    void removeGrabbedEdge();
    void reconnectGrabbedCable(const JackInfo& dst);

    ManualConnectionModel& m_connectionModel;
    JackHitMap& m_jackMap;
    std::function<bool()> m_isManualMode;

    DragMode m_dragMode = DragMode::None;
    juce::Point<float> m_dragStartPos;
    juce::Point<float> m_dragCurrentPos;
    int m_dragSrcJackIdx = -1;

    int m_grabbedEdgeIndex = -1;
    bool m_grabbingSrcEnd = false;
    juce::Point<float> m_anchoredPos;
    int m_grabbedSrcSlot = -1;
    int m_grabbedDstSlot = -1;
};
