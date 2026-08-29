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
    const bool isGrabDrag = m_routingCtrl.isGrabDrag();

    if (!(isGrabDrag && grabbedIdx == -1))
        m_renderer.drawInputJack(g, dawEntryPos(), m_cachedInputPath);

    m_renderer.drawRoutingCables(g, m_cachedConnectionPaths,
                                  isGrabDrag && grabbedIdx >= 0
                                  ? grabbedIdx : -1);

    if (isDragging && isGrabDrag)
        m_renderer.drawGrabbedCable(g, m_routingCtrl.anchoredPos(),
                                     m_routingCtrl.dragCurrentPos());

    if (!(isGrabDrag && grabbedIdx == -2))
        m_renderer.drawOutputJack(g, dawExitPos(), m_cachedOutputPath);

    if (isDragging && m_routingCtrl.isNewCableDrag())
        m_renderer.drawActiveDraggingCable(g, m_routingCtrl.dragStartPos(),
                                            m_routingCtrl.dragCurrentPos(),
                                            m_routingCtrl.dragSrcJackIdx());

    if (m_model.isManualMode() && isDragging)
    {
        const auto targets = m_routingCtrl.validTargetJackIndices();
        for (const int idx : targets)
            m_renderer.drawJackHighlight(g, m_jackMap.get(idx).pos);
    }
}

void PedalboardGrid::resized()
{
    m_layout.computeSlotBounds(getLocalBounds(), componentBounds());
    m_jackMap.refresh(componentBounds(), dawEntryPos(), dawExitPos(), getLocalBounds());
    rebuildCableCache();
    repaint();
}

void PedalboardGrid::updateRouting(const std::vector<uint8_t>& routingOrder)
{
    const bool changed = routingOrder != m_routingManager.getRoutingOrder();
    if (changed)
        m_routingManager.setRoutingOrder(routingOrder);

    rebuildCableCache();
    if (changed)
        repaint();
}

void PedalboardGrid::rebuildCableCache()
{
    rebuildConnectionCables();
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
    m_cachedInputPath.clear();
    m_cachedOutputPath.clear();

    constexpr int kRowGaps = GridLayout::ColCount - 1;
    constexpr int kVertChannels = kRowGaps;

    struct CableChannel
    {
        float pos = 0.0f;
        std::vector<int> users;
    };
    std::array<CableChannel, kVertChannels> vChannels;
    CableChannel hChannel;

    for (int gap = 0; gap < kRowGaps; ++gap)
    {
        const int leftSlot = gap;
        const int rightSlot = leftSlot + 1;
        const auto& lb = m_pedalComponents[static_cast<size_t>(leftSlot)]->getBounds();
        const auto& rb = m_pedalComponents[static_cast<size_t>(rightSlot)]->getBounds();
        vChannels[static_cast<size_t>(gap)].pos =
            (static_cast<float>(lb.getRight()) + static_cast<float>(rb.getX())) * 0.5f;
    }

    {
        const auto& top = m_pedalComponents[0]->getBounds();
        const auto& bottom = m_pedalComponents[static_cast<size_t>(GridLayout::ColCount)]->getBounds();
        hChannel.pos = (static_cast<float>(top.getBottom()) + static_cast<float>(bottom.getY())) * 0.5f;
    }

    struct Route
    {
        enum class Kind { SameRowTop, SameRowBottom, CrossRowTopDown, CrossRowBottomUp,
                          DawInTop, DawInBottom, DawOutTop, DawOutBottom };
        Kind kind = Kind::SameRowTop;
        juce::Point<float> p1;
        juce::Point<float> p2;
        int vA = -1;
        bool usesH = false;
        float lift = 0.0f;
        float laneA = 0.0f;
        float laneH = 0.0f;
    };
    std::vector<Route> routes;

    const auto registerRoute = [&](Route route) -> int
    {
        const int idx = static_cast<int>(routes.size());
        if (route.vA >= 0)
            vChannels[static_cast<size_t>(route.vA)].users.push_back(idx);
        if (route.usesH)
            hChannel.users.push_back(idx);
        routes.push_back(std::move(route));
        return idx;
    };

    const auto gapToward = [](int col, int dir)
    {
        return juce::jlimit(0, kRowGaps - 1, dir > 0 ? col : col - 1);
    };

    const auto addConnectionRoute = [&](int srcIdx, int dstIdx)
    {
        if (srcIdx < 0 || srcIdx >= PedalSlotCount || dstIdx < 0 || dstIdx >= PedalSlotCount)
            return;

        const auto p1 = m_pedalComponents[static_cast<size_t>(srcIdx)]->getOutputJackPos();
        const auto p2 = m_pedalComponents[static_cast<size_t>(dstIdx)]->getInputJackPos();
        const int srcRow = srcIdx / GridLayout::ColCount;
        const int dstRow = dstIdx / GridLayout::ColCount;
        const int srcCol = srcIdx % GridLayout::ColCount;
        const int dstCol = dstIdx % GridLayout::ColCount;

        Route route;
        route.p1 = p1;
        route.p2 = p2;
        route.lift = std::min(std::abs(p2.x - p1.x) * 0.30f, 70.0f);

        if (srcRow == dstRow)
        {
            if (srcRow == 0)
            {
                route.kind = Route::Kind::SameRowTop;
                route.vA = std::min(srcCol, dstCol);
            }
            else
            {
                route.kind = Route::Kind::SameRowBottom;
            }
        }
        else if (srcRow == 0)
        {
            route.kind = Route::Kind::CrossRowTopDown;
            route.vA = gapToward(srcCol, dstCol > srcCol ? 1 : -1);
            route.usesH = true;
        }
        else
        {
            route.kind = Route::Kind::CrossRowBottomUp;
            route.vA = gapToward(dstCol, srcCol > dstCol ? 1 : -1);
            route.usesH = true;
        }

        registerRoute(std::move(route));
    };

    const auto addDawRoute = [&](bool isInput, int pedalSlot)
    {
        if (pedalSlot < 0 || pedalSlot >= PedalSlotCount)
            return;

        const int row = pedalSlot / GridLayout::ColCount;
        const int col = pedalSlot % GridLayout::ColCount;
        const int gap = isInput ? std::min(col, kRowGaps - 1)
                                : std::max(col - (kRowGaps - 1), 0);

        Route route;
        route.p1 = isInput ? dawEntryPos()
                           : m_pedalComponents[static_cast<size_t>(pedalSlot)]->getOutputJackPos();
        route.p2 = isInput ? m_pedalComponents[static_cast<size_t>(pedalSlot)]->getInputJackPos()
                           : dawExitPos();
        route.vA = gap;
        route.lift = GridLayout::CableArcLiftPx;

        if (isInput)
        {
            route.kind = (row == 0) ? Route::Kind::DawInTop : Route::Kind::DawInBottom;
            if (row == 1)
                route.usesH = true;
        }
        else
        {
            route.kind = (row == 0) ? Route::Kind::DawOutTop : Route::Kind::DawOutBottom;
            if (row == 1)
                route.usesH = true;
        }

        registerRoute(std::move(route));
    };

    if (m_model.isManualMode())
    {
        for (const auto& edge : m_connectionModel.edges())
            addConnectionRoute(static_cast<int>(edge.first), static_cast<int>(edge.second));

        for (int p = 0; p < PedalSlotCount; ++p)
        {
            if (m_connectionModel.hasDawIn(static_cast<uint8_t>(p)))
                addDawRoute(true, p);
            if (m_connectionModel.hasDawOut(static_cast<uint8_t>(p)))
                addDawRoute(false, p);
        }
    }
    else
    {
        const auto connections = m_routingManager.getConnections();
        for (const auto& connection : connections)
            addConnectionRoute(static_cast<int>(connection.sourceSlot),
                               static_cast<int>(connection.destinationSlot));

        int firstActive = -1, lastActive = -1;
        for (int i = 0; i < PedalSlotCount; ++i)
            if (m_model.getPedalSlot(i) != DspModuleType::BYPASS)
            {
                if (firstActive == -1) firstActive = i;
                lastActive = i;
            }

        const auto& routing = m_routingManager.getRoutingOrder();
        const int firstSlot = routing.empty() ? firstActive : static_cast<int>(routing.front());
        const int lastSlot = routing.empty() ? lastActive : static_cast<int>(routing.back());
        addDawRoute(true, firstSlot);
        addDawRoute(false, lastSlot);
    }

    const float laneSpacing = GridLayout::CableLaneSpacingPx;
    const auto allocateLanes = [&](const std::vector<int>& users, const auto& laneSetter)
    {
        const int count = static_cast<int>(users.size());
        for (int i = 0; i < count; ++i)
        {
            const float offset = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * laneSpacing;
            laneSetter(users[static_cast<size_t>(i)], offset);
        }
    };

    for (size_t ci = 0; ci < vChannels.size(); ++ci)
    {
        const auto& ch = vChannels[ci];
        allocateLanes(ch.users, [&](int routeIdx, float offset)
        {
            routes[static_cast<size_t>(routeIdx)].laneA = offset;
        });
    }
    allocateLanes(hChannel.users, [&](int routeIdx, float offset)
    {
        routes[static_cast<size_t>(routeIdx)].laneH = offset;
    });

    const auto splitForCables = [&](const std::vector<juce::Point<float>>& waypoints,
                                    juce::Point<float> startTangent, juce::Point<float> endTangent)
    {
        auto split = CablePathBuilder::buildWaypointCable(waypoints, startTangent, endTangent);
        m_cachedConnectionPaths.push_back(std::move(split));
    };

    for (const auto& route : routes)
    {
        switch (route.kind)
        {
            case Route::Kind::SameRowTop:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                splitForCables(
                    { route.p1, { gapX, route.p1.y - route.lift }, route.p2 },
                    {0.0f, -1.0f}, {0.0f, 1.0f});
                break;
            }
            case Route::Kind::SameRowBottom:
            {
                const float midX = (route.p1.x + route.p2.x) * 0.5f;
                splitForCables(
                    { route.p1, { midX, route.p1.y - route.lift }, route.p2 },
                    {0.0f, -1.0f}, {0.0f, 1.0f});
                break;
            }
            case Route::Kind::CrossRowTopDown:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                const float hY = hChannel.pos + route.laneH;
                splitForCables(
                    { route.p1, { gapX, hY }, { route.p2.x, hY }, route.p2 },
                    {0.0f, -1.0f}, {0.0f, 1.0f});
                break;
            }
            case Route::Kind::CrossRowBottomUp:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                const float hY = hChannel.pos + route.laneH;
                splitForCables(
                    { route.p1, { route.p1.x, hY }, { gapX, hY },
                      { gapX, route.p2.y - route.lift }, route.p2 },
                    {0.0f, -1.0f}, {0.0f, 1.0f});
                break;
            }
            case Route::Kind::DawInTop:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                m_cachedInputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { gapX, route.p1.y + route.lift },
                      { gapX, route.p2.y - route.lift }, route.p2 },
                    {0.0f, 1.0f}, {0.0f, 1.0f});
                break;
            }
            case Route::Kind::DawInBottom:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                const float hY = hChannel.pos + route.laneH;
                m_cachedInputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { gapX, hY }, { route.p2.x, hY }, route.p2 },
                    {0.0f, 1.0f}, {0.0f, 1.0f});
                break;
            }
            case Route::Kind::DawOutTop:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                m_cachedOutputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { gapX, route.p1.y - route.lift },
                      { gapX, route.p2.y + route.lift }, route.p2 },
                    {0.0f, -1.0f}, {0.0f, -1.0f});
                break;
            }
            case Route::Kind::DawOutBottom:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                const float hY = hChannel.pos + route.laneH;
                m_cachedOutputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { route.p1.x, hY }, { gapX, hY },
                      { gapX, route.p2.y + route.lift }, route.p2 },
                    {0.0f, -1.0f}, {0.0f, -1.0f});
                break;
            }
        }
    }
}

void PedalboardGrid::syncPedals()
{
    for (auto& pedal : m_pedalComponents)
        if (pedal)
            pedal->syncFromProcessor();
    m_jackMap.refresh(componentBounds(), dawEntryPos(), dawExitPos(), getLocalBounds());
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
