#include "PedalboardGrid.h"
#include "Core/EditorDesignMetrics.h"
#include <algorithm>
#include <utility>

PedalboardGrid::PedalboardGrid(const EditorUiSnapshot& initialState,
                                   const IResourceProvider& resources,
                                   const ScaledAssetProvider& assets,
                                   const IThemeProvider& theme,
                                   CanvasRoutingManager& routingManager,
                                   Actions actions)
    : m_resources(resources),
      m_assets(assets),
      m_theme(theme),
      m_routingManager(routingManager),
      m_renderer(theme, m_resources, m_assets),
      m_routingCtrl(m_connectionModel, m_jackMap, [this]() { return m_manualMode; }),
      m_actions(std::move(actions)),
      m_manualMode(initialState.manualMode)
{
    addMouseListener(this, true);

    for (int s = 0; s < PedalSlotCount; ++s)
    {
        m_pedalTypes[static_cast<size_t>(s)] = initialState.pedals[static_cast<size_t>(s)].type;
        m_linked[static_cast<size_t>(s)] = initialState.pedals[static_cast<size_t>(s)].linked;
        m_linkMins[static_cast<size_t>(s)] = initialState.pedals[static_cast<size_t>(s)].linkRangeMins;
        m_linkMaxs[static_cast<size_t>(s)] = initialState.pedals[static_cast<size_t>(s)].linkRangeMaxs;
        PedalComponent::Actions actions;
        actions.setType = [this](int slot, DspModuleType type)
        {
            if (slot >= 0 && slot < PedalSlotCount)
            {
                m_pedalTypes[static_cast<size_t>(slot)] = type;
                m_linked[static_cast<size_t>(slot)].fill(false);
                m_linkMins[static_cast<size_t>(slot)].fill(0.0f);
                m_linkMaxs[static_cast<size_t>(slot)].fill(1.0f);
                if (auto* pedal = m_pedalComponents[static_cast<size_t>(slot)].get())
                    for (int k = 0; k < KnobsPerPedal; ++k)
                    {
                        pedal->setKnobLinked(k, false);
                        pedal->setKnobLinkRange(k, 0.0f, 1.0f);
                    }
            }
            if (m_actions.setPedalType)
                m_actions.setPedalType(slot, type);
            rebuildCableCache();
            repaint();
        };
        actions.setKnob = [this](int slot, int knob, float start, float value)
        {
            if (m_actions.setKnob)
                m_actions.setKnob(slot, knob, start, value);
        };
        actions.setLink = [this](int slot, int knob, bool linked)
        {
            if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < KnobsPerPedal)
            {
                m_linked[static_cast<size_t>(slot)][static_cast<size_t>(knob)] = linked;
                if (linked)
                {
                    m_linkMins[static_cast<size_t>(slot)][static_cast<size_t>(knob)] = 0.0f;
                    m_linkMaxs[static_cast<size_t>(slot)][static_cast<size_t>(knob)] = 1.0f;
                }
            }
            if (m_actions.setLink)
                m_actions.setLink(slot, knob, linked);
        };
        actions.setLinkRange = [this](int slot, int knob, float rMin, float rMax)
        {
            if (slot >= 0 && slot < PedalSlotCount && knob >= 0 && knob < KnobsPerPedal)
            {
                rMin = std::clamp(rMin, 0.0f, 1.0f);
                rMax = std::clamp(rMax, 0.0f, 1.0f);
                if (rMax < rMin + 0.05f) rMax = std::min(1.0f, rMin + 0.05f);
                if (rMin > rMax - 0.05f) rMin = std::max(0.0f, rMax - 0.05f);
                m_linkMins[static_cast<size_t>(slot)][static_cast<size_t>(knob)] = rMin;
                m_linkMaxs[static_cast<size_t>(slot)][static_cast<size_t>(knob)] = rMax;
            }
            if (m_actions.setLinkRange)
                m_actions.setLinkRange(slot, knob, rMin, rMax);
        };
        m_pedalComponents[static_cast<size_t>(s)] = std::make_unique<PedalComponent>(
            s, m_pedalTypes[static_cast<size_t>(s)], m_resources, m_assets, m_theme, std::move(actions));
        addAndMakeVisible(m_pedalComponents[static_cast<size_t>(s)].get());
    }
}

void PedalboardGrid::paintOverChildren(juce::Graphics& g)
{
    const bool isDragging = m_routingCtrl.isDragging();
    const int grabbedIdx = m_routingCtrl.grabbedEdgeIndex();
    const bool isGrabDrag = m_routingCtrl.isGrabDrag();
    const float jackH = dawJackHeight();

    m_renderer.drawInputJack(g, dawEntryPos(), m_cachedInputPath,
                              !(isGrabDrag && grabbedIdx == -1), jackH);

    m_renderer.drawRoutingCables(g, m_cachedConnectionPaths,
                                  isGrabDrag && grabbedIdx >= 0
                                  ? grabbedIdx : -1);

    if (isDragging && isGrabDrag)
        m_renderer.drawGrabbedCable(g, m_routingCtrl.anchoredPos(),
                                     m_routingCtrl.dragCurrentPos());

    m_renderer.drawOutputJack(g, dawExitPos(), m_cachedOutputPath,
                              !(isGrabDrag && grabbedIdx == -2), jackH);

    if (isDragging && m_routingCtrl.isNewCableDrag())
        m_renderer.drawActiveDraggingCable(g, m_routingCtrl.dragStartPos(),
                                            m_routingCtrl.dragCurrentPos(),
                                            m_routingCtrl.dragSrcJackIdx());

    if (m_manualMode && isDragging)
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

    const float maxCurve = juce::jlimit(
        EditorDesignMetrics::Cable::CurveMinPx,
        static_cast<float>(getHeight()) * EditorDesignMetrics::Cable::CurveBlobRatio,
        EditorDesignMetrics::Cable::CurveMaxPx);

    constexpr int kRowGaps = EditorDesignMetrics::ColCount - 1;
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
        const auto& bottom = m_pedalComponents[static_cast<size_t>(EditorDesignMetrics::ColCount)]->getBounds();
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
        const int srcRow = srcIdx / EditorDesignMetrics::ColCount;
        const int dstRow = dstIdx / EditorDesignMetrics::ColCount;
        const int srcCol = srcIdx % EditorDesignMetrics::ColCount;
        const int dstCol = dstIdx % EditorDesignMetrics::ColCount;

        Route route;
        route.p1 = p1;
        route.p2 = p2;
        route.lift = juce::jlimit(EditorDesignMetrics::Cable::JackRiseMinPx,
                                  std::abs(p2.x - p1.x) * EditorDesignMetrics::Cable::JackRiseSpanRatio,
                                  EditorDesignMetrics::Cable::JackRiseMaxPx);

        if (srcRow == dstRow)
        {
            if (srcRow == 0)
                route.kind = Route::Kind::SameRowTop;
            else
                route.kind = Route::Kind::SameRowBottom;
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

        const int row = pedalSlot / EditorDesignMetrics::ColCount;

        Route route;
        route.p1 = isInput ? dawEntryPos()
                           : m_pedalComponents[static_cast<size_t>(pedalSlot)]->getOutputJackPos();
        route.p2 = isInput ? m_pedalComponents[static_cast<size_t>(pedalSlot)]->getInputJackPos()
                           : dawExitPos();
        route.lift = EditorDesignMetrics::Cable::ArcLiftPx;

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

    if (m_manualMode)
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
            if (m_pedalTypes[static_cast<size_t>(i)] != DspModuleType::BYPASS)
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

    const float laneSpacing = EditorDesignMetrics::Cable::LaneSpacingPx;
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

    const auto splitForCables = [&](const std::vector<juce::Point<float>>& waypoints)
    {
        auto split = CablePathBuilder::buildWaypointCable(waypoints, maxCurve);
        m_cachedConnectionPaths.push_back(std::move(split));
    };

    for (const auto& route : routes)
    {
        switch (route.kind)
        {
            case Route::Kind::SameRowTop:
            {
                splitForCables(
                    { route.p1, { route.p1.x, route.p1.y - route.lift },
                      { route.p2.x, route.p1.y - route.lift }, route.p2 });
                break;
            }
            case Route::Kind::SameRowBottom:
            {
                const float hY = hChannel.pos;
                splitForCables(
                    { route.p1, { route.p1.x, hY },
                      { route.p2.x, hY }, route.p2 });
                break;
            }
            case Route::Kind::CrossRowTopDown:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                const float hY = hChannel.pos + route.laneH;
                const float bandY = route.p1.y - route.lift;
                splitForCables(
                    { route.p1, { route.p1.x, bandY }, { gapX, bandY },
                      { gapX, hY }, { route.p2.x, hY }, route.p2 });
                break;
            }
            case Route::Kind::CrossRowBottomUp:
            {
                const float gapX = vChannels[static_cast<size_t>(route.vA)].pos + route.laneA;
                const float hY = hChannel.pos + route.laneH;
                const float bandY = route.p2.y - route.lift;
                splitForCables(
                    { route.p1, { route.p1.x, hY }, { gapX, hY },
                      { gapX, bandY }, { route.p2.x, bandY }, route.p2 });
                break;
            }
            case Route::Kind::DawInTop:
            {
                const float bandY = route.p1.y + route.lift;
                m_cachedInputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { route.p1.x, bandY },
                      { route.p2.x, bandY }, route.p2 }, maxCurve);
                break;
            }
            case Route::Kind::DawInBottom:
            {
                const float hY = hChannel.pos + route.laneH;
                m_cachedInputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { route.p1.x, hY },
                      { route.p2.x, hY }, route.p2 }, maxCurve);
                break;
            }
            case Route::Kind::DawOutTop:
            {
                const float bandY = route.p2.y + route.lift;
                m_cachedOutputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { route.p1.x, bandY },
                      { route.p2.x, bandY }, route.p2 }, maxCurve);
                break;
            }
            case Route::Kind::DawOutBottom:
            {
                const float hY = hChannel.pos + route.laneH;
                m_cachedOutputPath = CablePathBuilder::buildWaypointPath(
                    { route.p1, { route.p1.x, hY },
                      { route.p2.x, hY }, route.p2 }, maxCurve);
                break;
            }
        }
    }
}

void PedalboardGrid::syncPedals()
{
    for (int slot = 0; slot < PedalSlotCount; ++slot)
        if (auto& pedal = m_pedalComponents[static_cast<size_t>(slot)])
        {
            pedal->syncType(m_pedalTypes[static_cast<size_t>(slot)]);
            for (int knob = 0; knob < KnobsPerPedal; ++knob)
            {
                pedal->setKnobLinked(knob, m_linked[static_cast<size_t>(slot)][static_cast<size_t>(knob)]);
                pedal->setKnobLinkRange(knob,
                                        m_linkMins[static_cast<size_t>(slot)][static_cast<size_t>(knob)],
                                        m_linkMaxs[static_cast<size_t>(slot)][static_cast<size_t>(knob)]);
            }
        }
    m_jackMap.refresh(componentBounds(), dawEntryPos(), dawExitPos(), getLocalBounds());
}

void PedalboardGrid::setViewState(const EditorUiSnapshot& state)
{
    m_manualMode = state.manualMode;
    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        m_pedalTypes[static_cast<size_t>(slot)] = state.pedals[static_cast<size_t>(slot)].type;
        m_linked[static_cast<size_t>(slot)] = state.pedals[static_cast<size_t>(slot)].linked;
        m_linkMins[static_cast<size_t>(slot)] = state.pedals[static_cast<size_t>(slot)].linkRangeMins;
        m_linkMaxs[static_cast<size_t>(slot)] = state.pedals[static_cast<size_t>(slot)].linkRangeMaxs;
        if (auto* pedal = m_pedalComponents[static_cast<size_t>(slot)].get())
        {
            for (int knob = 0; knob < KnobsPerPedal; ++knob)
                pedal->setKnobValue(knob, state.pedals[static_cast<size_t>(slot)].knobValues[static_cast<size_t>(knob)]);
        }
    }
    syncPedals();
    rebuildCableCache();
    repaint();
}

void PedalboardGrid::syncKnobLinkState(int slot, int knob, bool linked, float rangeMin, float rangeMax)
{
    if (slot < 0 || slot >= PedalSlotCount || knob < 0 || knob >= KnobsPerPedal)
        return;
    rangeMin = std::clamp(rangeMin, 0.0f, 1.0f);
    rangeMax = std::clamp(rangeMax, 0.0f, 1.0f);
    if (rangeMax < rangeMin + 0.05f) rangeMax = std::min(1.0f, rangeMin + 0.05f);
    if (rangeMin > rangeMax - 0.05f) rangeMin = std::max(0.0f, rangeMax - 0.05f);
    auto& l = m_linked[static_cast<size_t>(slot)][static_cast<size_t>(knob)];
    auto& mn = m_linkMins[static_cast<size_t>(slot)][static_cast<size_t>(knob)];
    auto& mx = m_linkMaxs[static_cast<size_t>(slot)][static_cast<size_t>(knob)];
    if (l == linked && mn == rangeMin && mx == rangeMax)
        return;
    l = linked;
    mn = rangeMin;
    mx = rangeMax;
    if (auto* pedal = m_pedalComponents[static_cast<size_t>(slot)].get())
    {
        pedal->setKnobLinked(knob, linked);
        pedal->setKnobLinkRange(knob, rangeMin, rangeMax);
    }
}

void PedalboardGrid::refreshAfterResize()
{
    for (auto& pedal : m_pedalComponents)
    {
        pedal->repaint();
    }
    repaint();
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
                               if (m_actions.setManualRouting)
                                   m_actions.setManualRouting(routing);
                          },
                          [this]() { rebuildCableCache(); });
    repaint();
}
