#include "ManualRoutingController.h"


ManualRoutingController::ManualRoutingController(ManualConnectionModel& connectionModel,
                                                  JackHitMap& jackMap,
                                                  IPedalboardModel& pedalModel)
    : m_connectionModel(connectionModel),
      m_jackMap(jackMap),
      m_pedalModel(pedalModel) {}

void ManualRoutingController::mouseDown(juce::Point<float> pos)
{
    if (!m_pedalModel.isManualMode())
        return;

    m_dragMode = DragMode::None;
    m_grabbedEdgeIndex = -1;
    m_grabbingSrcEnd = false;
    m_grabbedSrcSlot = -1;
    m_grabbedDstSlot = -1;
    m_dragSrcJackIdx = -1;

    const int jackIdx = m_jackMap.findAt(pos, m_jackMap.radius());

    if (jackIdx == -1)
        return;

    const auto& jack = m_jackMap.get(jackIdx);

    if (jack.pedalIdx == -1)
    {
        for (int p = 0; p < PedalSlotCount; ++p)
        {
            if (m_connectionModel.hasDawIn(static_cast<uint8_t>(p)))
            {
                m_dragMode = DragMode::GrabCable;
                m_grabbedEdgeIndex = -1;
                m_grabbingSrcEnd = true;
                m_grabbedSrcSlot = -1;
                m_grabbedDstSlot = p;
                // anchoredPos set by caller
                break;
            }
        }
    }
    else if (jack.pedalIdx == -2)
    {
        for (int p = 0; p < PedalSlotCount; ++p)
        {
            if (m_connectionModel.hasDawOut(static_cast<uint8_t>(p)))
            {
                m_dragMode = DragMode::GrabCable;
                m_grabbedEdgeIndex = -2;
                m_grabbingSrcEnd = false;
                m_grabbedSrcSlot = p;
                m_grabbedDstSlot = -2;
                break;
            }
        }
    }
    else if (jack.isInput)
    {
        const int p = jack.pedalIdx;
        if (m_connectionModel.hasDawIn(static_cast<uint8_t>(p)))
        {
            m_dragMode = DragMode::GrabCable;
            m_grabbedEdgeIndex = -1;
            m_grabbingSrcEnd = false;
            m_grabbedSrcSlot = -1;
            m_grabbedDstSlot = p;
        }
        else
        {
            for (size_t i = 0; i < m_connectionModel.edgeCount(); ++i)
            {
                if (m_connectionModel.edges()[i].second == static_cast<uint8_t>(p))
                {
                    m_dragMode = DragMode::GrabCable;
                    m_grabbedEdgeIndex = static_cast<int>(i);
                    m_grabbingSrcEnd = false;
                    m_grabbedSrcSlot = m_connectionModel.edges()[i].first;
                    m_grabbedDstSlot = p;
                    break;
                }
            }
        }
    }
    else
    {
        const int p = jack.pedalIdx;
        if (m_connectionModel.hasDawOut(static_cast<uint8_t>(p)))
        {
            m_dragMode = DragMode::GrabCable;
            m_grabbedEdgeIndex = -2;
            m_grabbingSrcEnd = true;
            m_grabbedSrcSlot = p;
            m_grabbedDstSlot = -2;
        }
        else
        {
            for (size_t i = 0; i < m_connectionModel.edgeCount(); ++i)
            {
                if (m_connectionModel.edges()[i].first == static_cast<uint8_t>(p))
                {
                    m_dragMode = DragMode::GrabCable;
                    m_grabbedEdgeIndex = static_cast<int>(i);
                    m_grabbingSrcEnd = true;
                    m_grabbedSrcSlot = p;
                    m_grabbedDstSlot = m_connectionModel.edges()[i].second;
                    break;
                }
            }
        }
    }

    if (m_dragMode != DragMode::GrabCable)
        m_dragMode = DragMode::NewCable;
    m_dragSrcJackIdx = jackIdx;
    m_dragStartPos = m_jackMap.get(jackIdx).pos;
    m_dragCurrentPos = pos;
}

void ManualRoutingController::mouseDrag(juce::Point<float> pos)
{
    if (m_dragMode != DragMode::None)
        m_dragCurrentPos = pos;
}

void ManualRoutingController::mouseUp(juce::Point<float> pos,
                                      std::function<void()> commitRouting,
                                      std::function<void()> rebuildCables)
{
    if (m_dragMode == DragMode::None)
        return;

    const int dstJackIdx = m_jackMap.findAt(pos, m_jackMap.radius());
    const bool wasGrabbing = (m_dragMode == DragMode::GrabCable);

    if (wasGrabbing && dstJackIdx != m_dragSrcJackIdx)
    {
        removeGrabbedEdge();
        if (dstJackIdx == -1)
        {
            commitRouting();
        }
        else
        {
            auto& dst = m_jackMap.get(dstJackIdx);
            reconnectGrabbedCable(dst);
        }
        rebuildCables();
        m_dragMode = DragMode::None;
        return;
    }
    m_dragMode = DragMode::None;

    auto& src = m_jackMap.get(m_dragSrcJackIdx);

    if (dstJackIdx != -1 && dstJackIdx != m_dragSrcJackIdx)
    {
        auto& dst = m_jackMap.get(dstJackIdx);

        if ((src.pedalIdx == -1 && dst.pedalIdx >= 0) || (dst.pedalIdx == -1 && src.pedalIdx >= 0))
        {
            int pd = (src.pedalIdx == -1) ? dst.pedalIdx : src.pedalIdx;
            m_connectionModel.removeEdgesWithDestination(static_cast<uint8_t>(pd));
            m_connectionModel.setDawIn(static_cast<uint8_t>(pd));
            commitRouting();
            rebuildCables();
        }
        else if ((src.pedalIdx >= 0 && dst.pedalIdx == -2) || (src.pedalIdx == -2 && dst.pedalIdx >= 0))
        {
            int pd = (src.pedalIdx == -2) ? dst.pedalIdx : src.pedalIdx;
            m_connectionModel.removeEdgesWithSource(static_cast<uint8_t>(pd));
            m_connectionModel.setDawOut(static_cast<uint8_t>(pd));
            commitRouting();
            rebuildCables();
        }
        else if (src.pedalIdx >= 0 && dst.pedalIdx >= 0 && src.pedalIdx != dst.pedalIdx)
        {
            int outPedal, inPedal;
            if (src.isInput != dst.isInput)
            {
                outPedal = src.isInput ? dst.pedalIdx : src.pedalIdx;
                inPedal  = src.isInput ? src.pedalIdx : dst.pedalIdx;
            }
            else
            {
                outPedal = -1;
                inPedal  = -1;
            }

            if (outPedal >= 0 && inPedal >= 0)
            {
                if (m_connectionModel.hasDawOut(static_cast<uint8_t>(outPedal)) ||
                    m_connectionModel.hasDawIn(static_cast<uint8_t>(inPedal)))
                {
                    rebuildCables();
                    return;
                }
                m_connectionModel.removeConflictingEdges(static_cast<uint8_t>(outPedal), static_cast<uint8_t>(inPedal));
                m_connectionModel.addEdge(static_cast<uint8_t>(outPedal), static_cast<uint8_t>(inPedal));
                commitRouting();
                rebuildCables();
            }
        }
    }
    else if (m_pedalModel.isManualMode() && src.pedalIdx >= 0 && dstJackIdx == -1 && !wasGrabbing)
    {
        uint8_t removedSlot = static_cast<uint8_t>(src.pedalIdx);
        m_connectionModel.removeAllEdgesForSlot(removedSlot);
        if (src.isInput)
            m_connectionModel.clearDawIn(removedSlot);
        else
            m_connectionModel.clearDawOut(removedSlot);
        commitRouting();
        rebuildCables();
    }
}

std::vector<int> ManualRoutingController::validTargetJackIndices() const
{
    std::vector<int> result;
    if (m_dragMode == DragMode::None || m_dragSrcJackIdx < 0)
        return result;

    const bool movingOutput = (m_dragMode == DragMode::NewCable)
        ? !m_jackMap.get(m_dragSrcJackIdx).isInput
        : m_grabbingSrcEnd;

    int movingEndPedal = -1;
    int anchoredPedal = -1;
    if (m_dragMode == DragMode::NewCable)
    {
        movingEndPedal = m_jackMap.get(m_dragSrcJackIdx).pedalIdx;
    }
    else
    {
        movingEndPedal = m_grabbingSrcEnd ? m_grabbedSrcSlot : m_grabbedDstSlot;
        anchoredPedal = m_grabbingSrcEnd ? m_grabbedDstSlot : m_grabbedSrcSlot;
    }

    const auto validPedal = [&](int pedal)
    {
        if (pedal < 0 || pedal >= PedalSlotCount)
            return false;
        if (pedal == movingEndPedal || pedal == anchoredPedal)
            return false;
        if (movingOutput && m_connectionModel.hasDawIn(static_cast<uint8_t>(pedal)))
            return false;
        if (!movingOutput && m_connectionModel.hasDawOut(static_cast<uint8_t>(pedal)))
            return false;
        return true;
    };

    for (int p = 0; p < PedalSlotCount; ++p)
    {
        if (!validPedal(p))
            continue;
        result.push_back(p * 2 + (movingOutput ? 0 : 1));
    }

    if (m_dragMode == DragMode::NewCable && m_jackMap.get(m_dragSrcJackIdx).pedalIdx >= 0)
        result.push_back(movingOutput ? JackHitMap::kJackCount - 1
                                      : JackHitMap::kJackCount - 2);

    return result;
}

void ManualRoutingController::removeGrabbedEdge()
{
    if (m_grabbedEdgeIndex == -1)
    {
        int p = m_grabbedDstSlot;
        if (p >= 0 && p < PedalSlotCount)
            m_connectionModel.clearDawIn(static_cast<uint8_t>(p));
    }
    else if (m_grabbedEdgeIndex == -2)
    {
        int p = m_grabbedSrcSlot;
        if (p >= 0 && p < PedalSlotCount)
            m_connectionModel.clearDawOut(static_cast<uint8_t>(p));
    }
    else if (m_grabbedEdgeIndex >= 0 && static_cast<size_t>(m_grabbedEdgeIndex) < m_connectionModel.edgeCount())
    {
        m_connectionModel.removeEdge(static_cast<size_t>(m_grabbedEdgeIndex));
    }
}

void ManualRoutingController::reconnectGrabbedCable(const JackInfo& dst)
{
    if (m_grabbedEdgeIndex == -1)
    {
        if (dst.pedalIdx < 0) return;
        m_connectionModel.setDawIn(static_cast<uint8_t>(dst.pedalIdx));
    }
    else if (m_grabbedEdgeIndex == -2)
    {
        if (dst.pedalIdx < 0) return;
        m_connectionModel.setDawOut(static_cast<uint8_t>(dst.pedalIdx));
    }
    else
    {
        if (dst.pedalIdx < 0) return;
        int outSlot = m_grabbedSrcSlot;
        int inSlot = m_grabbedDstSlot;
        if (m_grabbingSrcEnd)
            outSlot = dst.pedalIdx;
        else
            inSlot = dst.pedalIdx;
        if (outSlot >= 0 && inSlot >= 0 && outSlot != inSlot && outSlot < PedalSlotCount && inSlot < PedalSlotCount)
        {
            m_connectionModel.addEdge(static_cast<uint8_t>(outSlot), static_cast<uint8_t>(inSlot));
        }
    }
}
