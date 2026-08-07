#include "PedalboardGrid.h"
#include "GridLayout.h"

PedalboardGrid::PedalboardGrid(IPedalboardModel& model,
                                   const IResourceProvider& resources,
                                   const IThemeProvider& theme,
                                   CanvasRoutingManager& routingManager)
    : m_model(model),
      m_resources(resources),
      m_theme(theme),
      m_routingManager(routingManager),
      m_renderer(theme, m_resources),
      m_routingCtrl(m_connectionModel, m_jackMap, model)
{
    addMouseListener(this, true);

    for (int s = 0; s < PedalSlotCount; ++s)
    {
        m_pedalComponents[static_cast<size_t>(s)] = std::make_unique<PedalComponent>(
            m_model, s, m_model.getPedalSlot(s), m_resources, m_theme);
        addAndMakeVisible(m_pedalComponents[static_cast<size_t>(s)].get());
    }
}

void PedalboardGrid::paintOverChildren(juce::Graphics& g)
{
    const bool isDragging = m_routingCtrl.isDragging();
    const int grabbedIdx = m_routingCtrl.grabbedEdgeIndex();
    const bool isGrabbing = isDragging && grabbedIdx != -1;

    if (!isGrabbing || grabbedIdx != -1)
        m_renderer.drawInputJack(g, dawEntryPos(), m_cachedInputPath);

    m_renderer.drawRoutingCables(g, m_cachedConnectionPaths,
                                  isDragging && grabbedIdx >= 0
                                  ? grabbedIdx : -1);

    if (isDragging && grabbedIdx >= 0)
        m_renderer.drawGrabbedCable(g, m_routingCtrl.anchoredPos(),
                                     m_routingCtrl.dragCurrentPos());

    if (!isGrabbing || grabbedIdx != -2)
        m_renderer.drawOutputJack(g, dawExitPos(), m_cachedOutputPath);

    if (isDragging)
        m_renderer.drawActiveDraggingCable(g, m_routingCtrl.dragStartPos(),
                                            m_routingCtrl.dragCurrentPos(),
                                            m_routingCtrl.dragSrcJackIdx());
}

void PedalboardGrid::resized()
{
    m_layout.computeSlotBounds(getLocalBounds(), componentBounds());
    m_jackMap.refresh(componentBounds(), dawEntryPos(), dawExitPos());
    rebuildCableCache();
    repaint();
}

void PedalboardGrid::updateRouting(const std::vector<uint8_t>& routingOrder)
{
    if (routingOrder != m_routingManager.getRoutingOrder())
    {
        m_routingManager.setRoutingOrder(routingOrder);
        rebuildCableCache();
        repaint();
    }
}

void PedalboardGrid::rebuildCableCache()
{
    const auto& routing = m_routingManager.getRoutingOrder();

    if (!m_model.isManualMode())
    {
        int firstActive = -1, lastActive = -1;
        for (int i = 0; i < PedalSlotCount; ++i)
            if (m_model.getPedalSlot(i) != DspModuleType::BYPASS)
            {
                if (firstActive == -1) firstActive = i;
                lastActive = i;
            }

        m_cachedInputPath.clear();
        int firstSlot = routing.empty() ? firstActive : static_cast<int>(routing.front());
        if (firstSlot >= 0 && firstSlot < PedalSlotCount)
            buildInputCableTo(firstSlot);

        m_cachedOutputPath.clear();
        int lastSlot = routing.empty() ? lastActive : static_cast<int>(routing.back());
        if (lastSlot >= 0 && lastSlot < PedalSlotCount)
            buildOutputCableFrom(lastSlot);
    }

    rebuildConnectionCables();

    if (m_model.isManualMode())
    {
        m_cachedInputPath.clear();
        m_cachedOutputPath.clear();
        for (int p = 0; p < PedalSlotCount; ++p)
        {
            if (m_connectionModel.hasDawIn(static_cast<uint8_t>(p)))
                buildInputCableTo(p);
            if (m_connectionModel.hasDawOut(static_cast<uint8_t>(p)))
                buildOutputCableFrom(p);
        }
    }
}

void PedalboardGrid::restoreFromRouting(const std::vector<uint8_t>& routing)
{
    m_connectionModel.clear();
    if (!routing.empty())
    {
        m_connectionModel.setDawIn(routing.front());
        m_connectionModel.setDawOut(routing.back());
        for (size_t i = 0; i + 1 < routing.size(); ++i)
            m_connectionModel.addEdge(routing[i], routing[i + 1]);
    }
    rebuildCableCache();
}

void PedalboardGrid::rebuildConnectionCables()
{
    m_cachedConnectionPaths.clear();

    auto addCable = [this](int srcIdx, int dstIdx)
    {
        if (srcIdx < 0 || srcIdx >= PedalSlotCount || dstIdx < 0 || dstIdx >= PedalSlotCount)
            return;

        const auto p1 = m_pedalComponents[static_cast<size_t>(srcIdx)]->getOutputJackPos();
        const auto p2 = m_pedalComponents[static_cast<size_t>(dstIdx)]->getInputJackPos();
        const int srcRow = srcIdx / GridLayout::ColCount;
        const int dstRow = dstIdx / GridLayout::ColCount;

        if (srcRow == dstRow)
            buildSameRowCable(srcIdx, dstIdx, p1, p2);
        else
        {
            int colDelta = std::abs((dstIdx % GridLayout::ColCount) - (srcIdx % GridLayout::ColCount));
            if (colDelta <= 1)
                buildAdjacentColumnCable(srcIdx, dstIdx, p1, p2);
            else
                buildDistantColumnCable(srcIdx, dstIdx, p1, p2);
        }
    };

    if (m_model.isManualMode())
    {
        for (const auto& edge : m_connectionModel.edges())
            addCable(static_cast<int>(edge.first), static_cast<int>(edge.second));
    }
    else
    {
        const auto connections = m_routingManager.getConnections();
        for (const auto& connection : connections)
            addCable(static_cast<int>(connection.sourceSlot), static_cast<int>(connection.destinationSlot));
    }
}

void PedalboardGrid::buildSameRowCable(int, int, const juce::Point<float>& p1, const juce::Point<float>& p2)
{
    auto cps = CablePathBuilder::makeSameRowControlPoints(p1, p2);
    auto split = CablePathBuilder::splitCubicBezier(p1, cps.first, cps.second, p2);
    m_cachedConnectionPaths.push_back({ std::move(split.left), std::move(split.right) });
}

void PedalboardGrid::buildAdjacentColumnCable(int srcIdx, int dstIdx, const juce::Point<float>& p1, const juce::Point<float>& p2)
{
    auto srcBounds = m_pedalComponents[static_cast<size_t>(srcIdx)]->getBounds();
    auto dstBounds = m_pedalComponents[static_cast<size_t>(dstIdx)]->getBounds();
    float srcBotY = static_cast<float>(srcBounds.getBottom());
    float dstTopY = static_cast<float>(dstBounds.getY());
    float gapCenterY = (srcBotY + dstTopY) * 0.5f;

    int srcCol = srcIdx % GridLayout::ColCount;
    int dstCol = dstIdx % GridLayout::ColCount;
    int colDelta = std::abs(dstCol - srcCol);

    float gapX1, gapX2;
    if (colDelta == 0)
    {
        gapX1 = gapX2 = (p1.x + p2.x) * 0.5f;
    }
    else
    {
        int minCol = std::min(srcCol, dstCol);
        int row = srcIdx / GridLayout::ColCount;
        int leftSlot = minCol + row * GridLayout::ColCount;
        int rightSlot = minCol + 1 + row * GridLayout::ColCount;
        auto lBounds = m_pedalComponents[static_cast<size_t>(leftSlot)]->getBounds();
        auto rBounds = m_pedalComponents[static_cast<size_t>(rightSlot)]->getBounds();
        gapX1 = gapX2 = static_cast<float>((lBounds.getRight() + rBounds.getX()) / 2);
    }

    auto cp1 = juce::Point<float>{gapX1 + (p1.x > gapX1 ? 15.0f : -15.0f), gapCenterY};
    auto cp2 = juce::Point<float>{gapX2 + (p2.x > gapX2 ? 15.0f : -15.0f), gapCenterY};
    auto split = CablePathBuilder::splitCubicBezier(p1, cp1, cp2, p2);
    m_cachedConnectionPaths.push_back({ std::move(split.left), std::move(split.right) });
}

void PedalboardGrid::buildDistantColumnCable(int srcIdx, int dstIdx, const juce::Point<float>& p1, const juce::Point<float>& p2)
{
    auto srcBounds = m_pedalComponents[static_cast<size_t>(srcIdx)]->getBounds();
    auto dstBounds = m_pedalComponents[static_cast<size_t>(dstIdx)]->getBounds();
    float srcBotY = static_cast<float>(srcBounds.getBottom());
    float dstTopY = static_cast<float>(dstBounds.getY());
    float gapCenterY = (srcBotY + dstTopY) * 0.5f;

    int srcCol = srcIdx % GridLayout::ColCount;
    int srcRow = srcIdx / GridLayout::ColCount;
    int dstCol = dstIdx % GridLayout::ColCount;
    int dstRow = dstIdx / GridLayout::ColCount;

    int srcRightSlot = srcCol < GridLayout::ColCount - 1 ? srcIdx + 1 : -1;
    float gapX1;
    if (srcRightSlot >= 0 && srcRightSlot / GridLayout::ColCount == srcRow)
    {
        auto rBounds = m_pedalComponents[static_cast<size_t>(srcRightSlot)]->getBounds();
        gapX1 = static_cast<float>((srcBounds.getRight() + rBounds.getX()) / 2);
    }
    else
    {
        int leftSlot = srcIdx - 1;
        auto lBounds = m_pedalComponents[static_cast<size_t>(leftSlot)]->getBounds();
        gapX1 = static_cast<float>((lBounds.getRight() + srcBounds.getX()) / 2);
    }

    int dstRightSlot = dstCol < GridLayout::ColCount - 1 ? dstIdx + 1 : -1;
    float gapX2;
    if (dstRightSlot >= 0 && dstRightSlot / GridLayout::ColCount == dstRow)
    {
        auto rBounds = m_pedalComponents[static_cast<size_t>(dstRightSlot)]->getBounds();
        gapX2 = static_cast<float>((dstBounds.getRight() + rBounds.getX()) / 2);
    }
    else
    {
        int leftSlot = dstIdx - 1;
        auto lBounds = m_pedalComponents[static_cast<size_t>(leftSlot)]->getBounds();
        gapX2 = static_cast<float>((lBounds.getRight() + dstBounds.getX()) / 2);
    }

    auto cp1 = juce::Point<float>{gapX1 + (p1.x > gapX1 ? 15.0f : -15.0f), gapCenterY};
    auto cp2 = juce::Point<float>{gapX2 + (p2.x > gapX2 ? 15.0f : -15.0f), gapCenterY};
    auto split = CablePathBuilder::splitCubicBezier(p1, cp1, cp2, p2);
    m_cachedConnectionPaths.push_back({ std::move(split.left), std::move(split.right) });
}

void PedalboardGrid::buildInputCableTo(int pedalSlot)
{
    if (pedalSlot < 0 || pedalSlot >= PedalSlotCount)
        return;

    const auto* pedal = m_pedalComponents[static_cast<size_t>(pedalSlot)].get();
    if (!pedal)
        return;

    m_cachedInputPath = CablePathBuilder::buildInputCable(dawEntryPos(), pedal->getInputJackPos());
}

void PedalboardGrid::buildOutputCableFrom(int pedalSlot)
{
    if (pedalSlot < 0 || pedalSlot >= PedalSlotCount)
        return;

    const auto* pedal = m_pedalComponents[static_cast<size_t>(pedalSlot)].get();
    if (!pedal)
        return;

    m_cachedOutputPath = CablePathBuilder::buildOutputCable(pedal->getOutputJackPos(), dawExitPos());
}

void PedalboardGrid::syncPedals()
{
    for (auto& pedal : m_pedalComponents)
        if (pedal)
            pedal->syncFromProcessor();
    m_jackMap.refresh(componentBounds(), dawEntryPos(), dawExitPos());
}

void PedalboardGrid::mouseDown(const juce::MouseEvent& event)
{
    const auto pos = event.getEventRelativeTo(this).position;
    m_routingCtrl.mouseDown(pos);

    if (m_routingCtrl.isDragging())
    {
        if (m_routingCtrl.grabbedEdgeIndex() == -1)
        {
            int p = m_routingCtrl.grabbedDstSlot();
            if (p >= 0 && p < PedalSlotCount)
                m_routingCtrl.setAnchoredPos(m_pedalComponents[static_cast<size_t>(p)]->getInputJackPos());
            else
                m_routingCtrl.setAnchoredPos(dawEntryPos());
        }
        else if (m_routingCtrl.grabbedEdgeIndex() == -2)
        {
            int p = m_routingCtrl.grabbedSrcSlot();
            if (p >= 0 && p < PedalSlotCount)
                m_routingCtrl.setAnchoredPos(m_pedalComponents[static_cast<size_t>(p)]->getOutputJackPos());
            else
                m_routingCtrl.setAnchoredPos(dawExitPos());
        }
        else if (m_routingCtrl.grabbedEdgeIndex() >= 0)
        {
            if (m_routingCtrl.grabbingSrcEnd())
            {
                int p = m_routingCtrl.grabbedDstSlot();
                if (p >= 0 && p < PedalSlotCount)
                    m_routingCtrl.setAnchoredPos(m_pedalComponents[static_cast<size_t>(p)]->getInputJackPos());
            }
            else
            {
                int p = m_routingCtrl.grabbedSrcSlot();
                if (p >= 0 && p < PedalSlotCount)
                    m_routingCtrl.setAnchoredPos(m_pedalComponents[static_cast<size_t>(p)]->getOutputJackPos());
            }
        }

        repaint();
    }
}

void PedalboardGrid::mouseDrag(const juce::MouseEvent& event)
{
    if (m_routingCtrl.isDragging())
    {
        m_routingCtrl.mouseDrag(event.getEventRelativeTo(this).position);
        repaint();
    }
}

void PedalboardGrid::mouseUp(const juce::MouseEvent& event)
{
    m_routingCtrl.mouseUp(event.getEventRelativeTo(this).position,
                          [this]() {
                              auto routing = m_connectionModel.deriveRoutingOrder();
                              m_routingManager.setRoutingOrder(routing);
                              m_model.setManualRouting(routing);
                          },
                          [this]() { rebuildCableCache(); });
    repaint();
}
