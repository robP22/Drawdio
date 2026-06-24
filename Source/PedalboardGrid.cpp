#include "PedalboardGrid.h"
#include "GridLayout.h"
#include "PluginProcessor.h"
#include "RenderUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kJackRadius = 24.0f;

struct SplitCubic
{
    juce::Path left;
    juce::Path right;
};

SplitCubic splitCubicBezier(juce::Point<float> p0, juce::Point<float> p1,
                            juce::Point<float> p2, juce::Point<float> p3)
{
    const auto p01  = (p0 + p1) * 0.5f;
    const auto p12  = (p1 + p2) * 0.5f;
    const auto p23  = (p2 + p3) * 0.5f;
    const auto p012 = (p01 + p12) * 0.5f;
    const auto p123 = (p12 + p23) * 0.5f;
    const auto p0123 = (p012 + p123) * 0.5f;

    SplitCubic result;
    result.left.startNewSubPath(p0);
    result.left.cubicTo(p01, p012, p0123);
    result.right.startNewSubPath(p0123);
    result.right.cubicTo(p123, p23, p3);
    return result;
}

std::pair<juce::Point<float>, juce::Point<float>> makeSameRowControlPoints(
    juce::Point<float> from, juce::Point<float> to)
{
    const float h = std::abs(to.x - from.x);
    const float lift = std::min(h * 0.06f, 10.0f);
    const float curve = std::max(h * 0.28f, 20.0f);
    const float dir = (from.x < to.x) ? 1.0f : -1.0f;
    return {
        juce::Point<float>{from.x + curve * dir, from.y - lift},
        juce::Point<float>{to.x - curve * dir, to.y - lift}
    };
}

void renderCableSegment(juce::Graphics& g, const juce::Path& left, const juce::Path& right,
                        juce::Colour leftBase, juce::Colour rightBase)
{
    auto shadow = left;
    shadow.addPath(right);
    shadow.applyTransform(juce::AffineTransform::translation(3.0f, 7.0f));
    RenderUtils::strokeCable(g, shadow, juce::Colours::black.withAlpha(0.45f), 8.0f);
    if (!left.isEmpty())
    {
        RenderUtils::strokeCable(g, left, leftBase.darker(0.18f), 6.2f);
        RenderUtils::strokeCable(g, left, leftBase, 4.8f);
        RenderUtils::strokeCable(g, left, juce::Colours::white.withAlpha(0.14f), 1.4f);
    }
    if (!right.isEmpty())
    {
        RenderUtils::strokeCable(g, right, rightBase.darker(0.18f), 6.2f);
        RenderUtils::strokeCable(g, right, rightBase, 4.8f);
        RenderUtils::strokeCable(g, right, juce::Colours::white.withAlpha(0.14f), 1.4f);
    }
}
}

PedalboardGrid::PedalboardGrid(DrawdioProcessor& processor,
                                   const ResourceManager& resources,
                                   const IThemeProvider& theme,
                                   CanvasRoutingManager& routingManager)
    : audioProcessor(processor),
      m_resources(resources),
      m_theme(theme),
      m_routingManager(routingManager)
{
    addMouseListener(this, true);

    for (int s = 0; s < PedalSlotCount; ++s)
    {
        m_pedalComponents[static_cast<size_t>(s)] = std::make_unique<PedalComponent>(
            audioProcessor, s, audioProcessor.getPedalSlot(s), m_resources, m_theme);
        addAndMakeVisible(m_pedalComponents[static_cast<size_t>(s)].get());
    }
}

void PedalboardGrid::paint(juce::Graphics& g)
{
    if (m_dragMode != DragMode::GrabCable || m_grabbedEdgeIndex != -1)
        drawInputCable(g);

    drawRoutingCables(g);

    if (m_dragMode == DragMode::GrabCable)
        drawGrabbedCable(g);

    if (m_dragMode != DragMode::GrabCable || m_grabbedEdgeIndex != -2)
        drawOutputCable(g);

    if (m_dragMode != DragMode::GrabCable)
        drawActiveDraggingCable(g);
}

void PedalboardGrid::resized()
{
    const float sidePad = getWidth() * GridLayout::GridSidePaddingRatio;
    const float topPad = getHeight() * GridLayout::GridTopPaddingRatio;
    auto bounds = getLocalBounds().withTrimmedLeft(juce::roundToInt(sidePad))
                                        .withTrimmedRight(juce::roundToInt(sidePad))
                                        .withTrimmedTop(juce::roundToInt(topPad))
                                        .withTrimmedBottom(juce::roundToInt(topPad));

    const float colGap = bounds.getWidth() * GridLayout::ColumnGapRatio;
    const float rowGap = bounds.getHeight() * GridLayout::RowGapRatio;

    const float pedalWUnclamped = (bounds.getWidth() / GridLayout::ColCount - colGap) * GridLayout::PedalShrinkRatio;
    const float pedalHUnclamped = (bounds.getHeight() / GridLayout::RowCount - rowGap) * GridLayout::PedalShrinkRatio;

    const int pedalW = juce::roundToInt(juce::jlimit(
        bounds.getWidth() * GridLayout::PedalWidthMinRatio,
        bounds.getWidth() * GridLayout::PedalWidthMaxRatio,
        pedalWUnclamped));
    const int pedalH = juce::roundToInt(juce::jlimit(
        bounds.getHeight() * GridLayout::PedalHeightMinRatio,
        bounds.getHeight() * GridLayout::PedalHeightMaxRatio,
        pedalHUnclamped));

    const int groupW  = pedalW * GridLayout::ColCount + juce::roundToInt(colGap * (GridLayout::ColCount - 1));
    const int xOrigin = bounds.getX() + (bounds.getWidth() - groupW) / 2;

    const int groupH  = pedalH * GridLayout::RowCount + juce::roundToInt(rowGap * (GridLayout::RowCount - 1));
    const int yOrigin = bounds.getY() + (bounds.getHeight() - groupH) / 2
                        + juce::roundToInt(bounds.getHeight() * GridLayout::VerticalGroupOffsetRatio);

    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        const int row = slot / GridLayout::ColCount;
        const int col = slot % GridLayout::ColCount;

        const int x = xOrigin + col * (pedalW + juce::roundToInt(colGap));
        const int y = yOrigin + row * (pedalH + juce::roundToInt(rowGap));

        m_pedalComponents[static_cast<size_t>(slot)]->setBounds(x, y, pedalW, pedalH);
    }

    refreshJacks();
    rebuildCableCache();
}

void PedalboardGrid::refreshJacks()
{
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        m_cachedJacks[static_cast<size_t>(i) * 2]     = { i, true,  m_pedalComponents[static_cast<size_t>(i)]->getInputJackPos() };
        m_cachedJacks[static_cast<size_t>(i) * 2 + 1] = { i, false, m_pedalComponents[static_cast<size_t>(i)]->getOutputJackPos() };
    }
    m_cachedJacks[static_cast<size_t>(PedalSlotCount * 2)]     = { -1, false, dawEntryPos() };
    m_cachedJacks[static_cast<size_t>(PedalSlotCount * 2 + 1)] = { -2, true,  dawExitPos() };
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

    int firstActive = -1, lastActive = -1;
    for (int i = 0; i < PedalSlotCount; ++i)
        if (audioProcessor.getPedalSlot(i) != DspModuleType::BYPASS)
        {
            if (firstActive == -1) firstActive = i;
            lastActive = i;
        }

    // --- Input cable ---
    if (!audioProcessor.isManualMode())
    {
        m_cachedInputPath.clear();
        int firstSlot = routing.empty() ? firstActive : static_cast<int>(routing.front());
        if (firstSlot >= 0 && firstSlot < PedalSlotCount)
            buildInputCableTo(firstSlot);
    }

    // --- Output cable ---
    if (!audioProcessor.isManualMode())
    {
        m_cachedOutputPath.clear();
        int lastSlot = routing.empty() ? lastActive : static_cast<int>(routing.back());
        if (lastSlot >= 0 && lastSlot < PedalSlotCount)
            buildOutputCableFrom(lastSlot);
    }

    // --- Connection cables ---
    m_cachedConnectionPaths.clear();

    auto buildConnectionCable = [&](int srcIdx, int dstIdx)
    {
        if (srcIdx < 0 || srcIdx >= PedalSlotCount || dstIdx < 0 || dstIdx >= PedalSlotCount)
            return;

        const auto p1 = m_pedalComponents[static_cast<size_t>(srcIdx)]->getOutputJackPos();
        const auto p2 = m_pedalComponents[static_cast<size_t>(dstIdx)]->getInputJackPos();
        const float horizontal = std::abs(p2.x - p1.x);
        const int srcRow = srcIdx / GridLayout::ColCount;
        const int dstRow = dstIdx / GridLayout::ColCount;

        juce::Point<float> cp1, cp2;

        if (srcRow == dstRow)
        {
            auto cps = makeSameRowControlPoints(p1, p2);
            cp1 = cps.first;
            cp2 = cps.second;
        }
        else
        {
            auto srcBounds = m_pedalComponents[static_cast<size_t>(srcIdx)]->getBounds();
            auto dstBounds = m_pedalComponents[static_cast<size_t>(dstIdx)]->getBounds();
            float srcBotY = srcBounds.getBottom();
            float dstTopY = dstBounds.getY();
            float gapCenterY = (srcBotY + dstTopY) * 0.5f;

            int srcCol = srcIdx % GridLayout::ColCount;
            int dstCol = dstIdx % GridLayout::ColCount;
            int colDelta = std::abs(dstCol - srcCol);
            int srcRow = srcIdx / GridLayout::ColCount;

            float gapX1, gapX2;
            if (colDelta == 0)
            {
                gapX1 = gapX2 = (p1.x + p2.x) * 0.5f;
            }
            else if (colDelta == 1)
            {
                int minCol = std::min(srcCol, dstCol);
                int leftSlot = minCol + srcRow * GridLayout::ColCount;
                int rightSlot = minCol + 1 + srcRow * GridLayout::ColCount;
                auto lBounds = m_pedalComponents[static_cast<size_t>(leftSlot)]->getBounds();
                auto rBounds = m_pedalComponents[static_cast<size_t>(rightSlot)]->getBounds();
                gapX1 = gapX2 = (lBounds.getRight() + rBounds.getX()) * 0.5f;
            }
            else
            {
                int srcRightSlot = srcCol < GridLayout::ColCount - 1 ? srcIdx + 1 : -1;
                if (srcRightSlot >= 0 && srcRightSlot / GridLayout::ColCount == srcRow)
                {
                    auto rBounds = m_pedalComponents[static_cast<size_t>(srcRightSlot)]->getBounds();
                    gapX1 = (srcBounds.getRight() + rBounds.getX()) * 0.5f;
                }
                else
                {
                    int leftSlot = srcIdx - 1;
                    auto lBounds = m_pedalComponents[static_cast<size_t>(leftSlot)]->getBounds();
                    gapX1 = (lBounds.getRight() + srcBounds.getX()) * 0.5f;
                }
                int dstRow = dstIdx / GridLayout::ColCount;
                int dstRightSlot = dstCol < GridLayout::ColCount - 1 ? dstIdx + 1 : -1;
                if (dstRightSlot >= 0 && dstRightSlot / GridLayout::ColCount == dstRow)
                {
                    auto rBounds = m_pedalComponents[static_cast<size_t>(dstRightSlot)]->getBounds();
                    gapX2 = (dstBounds.getRight() + rBounds.getX()) * 0.5f;
                }
                else
                {
                    int leftSlot = dstIdx - 1;
                    auto lBounds = m_pedalComponents[static_cast<size_t>(leftSlot)]->getBounds();
                    gapX2 = (lBounds.getRight() + dstBounds.getX()) * 0.5f;
                }
            }

            cp1 = {gapX1 + (p1.x > gapX1 ? 15.0f : -15.0f), gapCenterY};
            cp2 = {gapX2 + (p2.x > gapX2 ? 15.0f : -15.0f), gapCenterY};
        }

        auto split = splitCubicBezier(p1, cp1, cp2, p2);
        m_cachedConnectionPaths.push_back({ std::move(split.left), std::move(split.right) });
    };

    if (audioProcessor.isManualMode())
    {
        for (const auto& edge : m_connectionModel.edges())
            buildConnectionCable(static_cast<int>(edge.first), static_cast<int>(edge.second));

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
    else
    {
        const auto connections = m_routingManager.getConnections();
        for (const auto& connection : connections)
            buildConnectionCable(static_cast<int>(connection.sourceSlot), static_cast<int>(connection.destinationSlot));
    }
}

void PedalboardGrid::buildInputCableTo(int pedalSlot)
{
    if (pedalSlot < 0 || pedalSlot >= PedalSlotCount)
        return;

    const auto* pedal = m_pedalComponents[static_cast<size_t>(pedalSlot)].get();
    if (!pedal)
        return;

    const auto jackPos = pedal->getInputJackPos();
    const auto entryPos = dawEntryPos();
    const float vertDist = jackPos.y - entryPos.y;
    const float lift = vertDist * 0.4f;
    const float offsetX = 30.0f;

    m_cachedInputPath.startNewSubPath(entryPos);
    m_cachedInputPath.cubicTo(entryPos.x + offsetX, entryPos.y + lift,
                              jackPos.x + offsetX, jackPos.y - lift,
                              jackPos.x, jackPos.y);
}

void PedalboardGrid::buildOutputCableFrom(int pedalSlot)
{
    if (pedalSlot < 0 || pedalSlot >= PedalSlotCount)
        return;

    const auto* pedal = m_pedalComponents[static_cast<size_t>(pedalSlot)].get();
    if (!pedal)
        return;

    const auto jackPos = pedal->getOutputJackPos();
    const auto exitPos = dawExitPos();
    const float vertDist = jackPos.y - exitPos.y;
    const float lift = vertDist * 0.4f;
    const float offsetX = 30.0f;

    m_cachedOutputPath.startNewSubPath(jackPos);
    m_cachedOutputPath.cubicTo(jackPos.x - offsetX, jackPos.y - lift,
                               exitPos.x - offsetX, exitPos.y + lift,
                                exitPos.x, exitPos.y);
}

void PedalboardGrid::syncPedals()
{
    for (auto& pedal : m_pedalComponents)
        if (pedal)
            pedal->syncFromProcessor();
    refreshJacks();
}

void PedalboardGrid::drawRoutingCables(juce::Graphics& g)
{
    for (size_t i = 0; i < m_cachedConnectionPaths.size(); ++i)
    {
        if (m_dragMode == DragMode::GrabCable && m_grabbedEdgeIndex >= 0 && static_cast<int>(i) == m_grabbedEdgeIndex)
            continue;

        const auto& cable = m_cachedConnectionPaths[i];
        renderCableSegment(g, cable.left, cable.right,
                           m_theme.cableOutColour(), m_theme.cableInColour());
    }
}

void PedalboardGrid::drawActiveDraggingCable(juce::Graphics& g)
{
    if (m_dragMode != DragMode::NewCable)
        return;

    const auto p1 = m_dragStartPos;
    const auto p2 = m_dragCurrentPos;
    const float horizontal = std::abs(p2.x - p1.x);

    juce::Point<float> cp1, cp2;

    if (m_dragSrcJackIdx >= 0)
    {
        float curveX = std::max(horizontal * 0.35f, 20.0f);
        float lift = std::min(horizontal * 0.04f + 4.0f, 15.0f);
        cp1 = {p1.x + curveX, p1.y - lift};
        cp2 = {p2.x - curveX, p2.y - lift};
    }
    else
    {
        float lift = 32.0f + horizontal * 0.06f;
        float curve = std::max(horizontal * 0.34f, 44.0f);
        cp1 = {p1.x + curve, p1.y - lift};
        cp2 = {p2.x - curve, p2.y - lift};
    }

    auto split = splitCubicBezier(p1, cp1, cp2, p2);

    RenderUtils::strokeCable(g, split.left, m_theme.cableOutColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.left, juce::Colours::white.withAlpha(0.16f), 1.2f);
    RenderUtils::strokeCable(g, split.right, m_theme.cableInColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.right, juce::Colours::white.withAlpha(0.16f), 1.2f);
}

void PedalboardGrid::drawGrabbedCable(juce::Graphics& g)
{
    if (m_dragMode != DragMode::GrabCable)
        return;

    juce::Point<float> fromPos, toPos;
    if (m_grabbingSrcEnd)
    {
        fromPos = m_dragCurrentPos;
        toPos = m_anchoredPos;
    }
    else
    {
        fromPos = m_anchoredPos;
        toPos = m_dragCurrentPos;
    }

    auto cps = makeSameRowControlPoints(fromPos, toPos);
    auto split = splitCubicBezier(fromPos, cps.first, cps.second, toPos);
    renderCableSegment(g, split.left, split.right,
                       m_theme.cableOutColour(), m_theme.cableInColour());
}

void PedalboardGrid::drawInputCable(juce::Graphics& g)
{
    const auto entryPos = dawEntryPos();
    constexpr float jackR = 7.0f;

    g.setColour(juce::Colours::dimgrey);
    g.fillEllipse(entryPos.x - jackR, entryPos.y - jackR, jackR * 2.0f, jackR * 2.0f);
    g.setColour(juce::Colours::silver);
    g.drawEllipse(entryPos.x - jackR, entryPos.y - jackR, jackR * 2.0f, jackR * 2.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("IN", entryPos.x + jackR + 4.0f, entryPos.y - 9.0f, 30.0f, 18.0f,
               juce::Justification::centredLeft, false);

    if (m_cachedInputPath.isEmpty())
        return;

    juce::Path empty;
    renderCableSegment(g, empty, m_cachedInputPath,
                       juce::Colours::transparentBlack, m_theme.cableInColour());
}

void PedalboardGrid::drawOutputCable(juce::Graphics& g)
{
    const auto exitPos = dawExitPos();
    constexpr float jackR = 7.0f;

    g.setColour(juce::Colours::dimgrey);
    g.fillEllipse(exitPos.x - jackR, exitPos.y - jackR, jackR * 2.0f, jackR * 2.0f);
    g.setColour(juce::Colours::silver);
    g.drawEllipse(exitPos.x - jackR, exitPos.y - jackR, jackR * 2.0f, jackR * 2.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("OUT", exitPos.x - jackR - 30.0f - 4.0f, exitPos.y - 9.0f, 30.0f, 18.0f,
               juce::Justification::centredRight, false);

    if (m_cachedOutputPath.isEmpty())
        return;

    juce::Path empty;
    renderCableSegment(g, m_cachedOutputPath, empty,
                       m_theme.cableOutColour(), juce::Colours::transparentBlack);
}

void PedalboardGrid::mouseDown(const juce::MouseEvent& event)
{
    if (!audioProcessor.isManualMode())
        return;

    const auto pos = event.getEventRelativeTo(this).position;
    const int jackIdx = findJackAt(pos, kJackRadius);

    if (jackIdx != -1)
    {
        // Check if this jack already has a cable attached → grab it
        const auto& jack = m_cachedJacks[static_cast<size_t>(jackIdx)];

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
                    m_anchoredPos = m_pedalComponents[static_cast<size_t>(p)]->getInputJackPos();
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
                    m_anchoredPos = m_pedalComponents[static_cast<size_t>(p)]->getOutputJackPos();
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
                m_anchoredPos = dawEntryPos();
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
                        m_anchoredPos = m_pedalComponents[static_cast<size_t>(m_connectionModel.edges()[i].first)]->getOutputJackPos();
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
                m_anchoredPos = dawExitPos();
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
                        m_anchoredPos = m_pedalComponents[static_cast<size_t>(m_connectionModel.edges()[i].second)]->getInputJackPos();
                        break;
                    }
                }
            }
        }

        if (m_dragMode != DragMode::GrabCable)
            m_dragMode = DragMode::NewCable;
        m_dragSrcJackIdx = jackIdx;
        m_dragStartPos = m_cachedJacks[static_cast<size_t>(jackIdx)].pos;
        m_dragCurrentPos = pos;
        repaint();
    }
}

void PedalboardGrid::mouseDrag(const juce::MouseEvent& event)
{
    if (m_dragMode != DragMode::None)
    {
        m_dragCurrentPos = event.getEventRelativeTo(this).position;
        repaint();
    }
}

void PedalboardGrid::mouseUp(const juce::MouseEvent& event)
{
    if (m_dragMode == DragMode::None)
        return;

    const int dstJackIdx = findJackAt(event.getEventRelativeTo(this).position, kJackRadius);
    const bool wasGrabbing = (m_dragMode == DragMode::GrabCable);

    // Phase 1: if grabbing and dropping on a different jack or empty space, remove the old edge and resolve
    if (wasGrabbing && dstJackIdx != m_dragSrcJackIdx)
    {
        removeGrabbedEdge();
        if (dstJackIdx == -1)
        {
        m_connectionModel.commitTo(m_routingManager, audioProcessor);
        }
        else
        {
            auto& dst = m_cachedJacks[static_cast<size_t>(dstJackIdx)];
            reconnectGrabbedCable(dst);
        }
        rebuildCableCache();
        repaint();
        m_dragMode = DragMode::None;
        return;
    }
    m_dragMode = DragMode::None;

    // Phase 2: existing routing resolution (non-grab path)
    auto& src = m_cachedJacks[static_cast<size_t>(m_dragSrcJackIdx)];

    if (dstJackIdx != -1 && dstJackIdx != m_dragSrcJackIdx)
    {
        auto& dst = m_cachedJacks[static_cast<size_t>(dstJackIdx)];

        if ((src.pedalIdx == -1 && dst.pedalIdx >= 0) || (dst.pedalIdx == -1 && src.pedalIdx >= 0))
        {
            int pd = (src.pedalIdx == -1) ? dst.pedalIdx : src.pedalIdx;
            m_connectionModel.removeEdgesWithDestination(static_cast<uint8_t>(pd));
            m_connectionModel.setDawIn(static_cast<uint8_t>(pd));
            m_connectionModel.commitTo(m_routingManager, audioProcessor);
            m_cachedInputPath.clear();
            buildInputCableTo(pd);
            rebuildCableCache();
        }
        else if ((src.pedalIdx >= 0 && dst.pedalIdx == -2) || (src.pedalIdx == -2 && dst.pedalIdx >= 0))
        {
            int pd = (src.pedalIdx == -2) ? dst.pedalIdx : src.pedalIdx;
            m_connectionModel.removeEdgesWithSource(static_cast<uint8_t>(pd));
            m_connectionModel.setDawOut(static_cast<uint8_t>(pd));
            m_connectionModel.commitTo(m_routingManager, audioProcessor);
            m_cachedOutputPath.clear();
            buildOutputCableFrom(pd);
            rebuildCableCache();
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
                    rebuildCableCache();
                    repaint();
                    return;
                }
                m_connectionModel.removeConflictingEdges(static_cast<uint8_t>(outPedal), static_cast<uint8_t>(inPedal));
                m_connectionModel.addEdge(static_cast<uint8_t>(outPedal), static_cast<uint8_t>(inPedal));
                m_connectionModel.commitTo(m_routingManager, audioProcessor);
                rebuildCableCache();
            }
        }
    }
    else if (audioProcessor.isManualMode() && src.pedalIdx >= 0 && dstJackIdx == -1 && !wasGrabbing)
    {
        uint8_t removedSlot = static_cast<uint8_t>(src.pedalIdx);
        m_connectionModel.removeAllEdgesForSlot(removedSlot);
        m_connectionModel.clearDawIn(removedSlot);
        m_connectionModel.clearDawOut(removedSlot);
        auto routing = m_connectionModel.deriveRoutingOrder();
        if (routing != m_routingManager.getRoutingOrder())
        {
            m_connectionModel.commitTo(m_routingManager, audioProcessor);
            if (routing.empty())
            {
                m_cachedInputPath.clear();
                m_cachedOutputPath.clear();
            }
            rebuildCableCache();
        }
    }

    repaint();
}

void PedalboardGrid::removeGrabbedEdge()
{
    if (m_grabbedEdgeIndex == -1)
    {
        int p = m_grabbedDstSlot;
        if (p >= 0 && p < PedalSlotCount)
        {
            m_connectionModel.clearDawIn(static_cast<uint8_t>(p));
            m_cachedInputPath.clear();
        }
    }
    else if (m_grabbedEdgeIndex == -2)
    {
        int p = m_grabbedSrcSlot;
        if (p >= 0 && p < PedalSlotCount)
        {
            m_connectionModel.clearDawOut(static_cast<uint8_t>(p));
            m_cachedOutputPath.clear();
        }
    }
    else if (m_grabbedEdgeIndex >= 0 && static_cast<size_t>(m_grabbedEdgeIndex) < m_connectionModel.edgeCount())
    {
        m_connectionModel.removeEdge(static_cast<size_t>(m_grabbedEdgeIndex));
    }
}

void PedalboardGrid::reconnectGrabbedCable(const JackInfo& dst)
{
    if (m_grabbedEdgeIndex == -1)
    {
        if (dst.pedalIdx < 0) return;
        int p = dst.pedalIdx;
        m_connectionModel.setDawIn(static_cast<uint8_t>(p));
        m_connectionModel.commitTo(m_routingManager, audioProcessor);
        buildInputCableTo(p);
    }
    else if (m_grabbedEdgeIndex == -2)
    {
        if (dst.pedalIdx < 0) return;
        int p = dst.pedalIdx;
        m_connectionModel.setDawOut(static_cast<uint8_t>(p));
        m_connectionModel.commitTo(m_routingManager, audioProcessor);
        buildOutputCableFrom(p);
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
            m_connectionModel.commitTo(m_routingManager, audioProcessor);
        }
    }
}

int PedalboardGrid::findJackAt(juce::Point<float> pos, float radius) const
{
    for (int i = 0; i < PedalSlotCount * 2 + 2; ++i)
        if (m_cachedJacks[static_cast<size_t>(i)].pos.getDistanceFrom(pos) <= radius)
            return i;

    return -1;
}
