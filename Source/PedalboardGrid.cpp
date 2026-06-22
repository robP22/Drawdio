#include "PedalboardGrid.h"
#include "GridLayout.h"
#include "PluginProcessor.h"
#include "RenderUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kJackRadius = 16.0f;

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
    drawInputCable(g);
    drawRoutingCables(g);
    drawOutputCable(g);
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
}

void PedalboardGrid::refreshJacks()
{
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        m_cachedJacks[i * 2]     = { i, true,  m_pedalComponents[static_cast<size_t>(i)]->getInputJackPos() };
        m_cachedJacks[i * 2 + 1] = { i, false, m_pedalComponents[static_cast<size_t>(i)]->getOutputJackPos() };
    }
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

    // --- Input cable ---
    m_cachedInputPath.clear();
    if (!routing.empty())
    {
        const int firstSlot = routing.front();
        if (firstSlot >= 0 && firstSlot < PedalSlotCount)
        {
            const auto* pedal = m_pedalComponents[static_cast<size_t>(firstSlot)].get();
            if (pedal)
            {
                const auto jackPos = pedal->getInputJackPos();
                const juce::Point<float> entryPos(static_cast<float>(getWidth()) * 0.05f, 0.0f);
                const float vertDist = jackPos.y - entryPos.y;
                const float lift = vertDist * 0.4f;
                const float offsetX = 30.0f;

                m_cachedInputPath.startNewSubPath(entryPos);
                m_cachedInputPath.cubicTo(entryPos.x + offsetX, entryPos.y + lift,
                                          jackPos.x + offsetX, jackPos.y - lift,
                                          jackPos.x, jackPos.y);
            }
        }
    }

    // --- Output cable ---
    m_cachedOutputPath.clear();
    if (!routing.empty())
    {
        const int lastSlot = routing.back();
        if (lastSlot >= 0 && lastSlot < PedalSlotCount)
        {
            const auto* pedal = m_pedalComponents[static_cast<size_t>(lastSlot)].get();
            if (pedal)
            {
                const auto jackPos = pedal->getOutputJackPos();
                const juce::Point<float> exitPos(static_cast<float>(getWidth()) * 0.95f, 0.0f);
                const float vertDist = jackPos.y - exitPos.y;
                const float lift = vertDist * 0.4f;
                const float offsetX = 30.0f;

                m_cachedOutputPath.startNewSubPath(jackPos);
                m_cachedOutputPath.cubicTo(jackPos.x - offsetX, jackPos.y - lift,
                                           exitPos.x - offsetX, exitPos.y + lift,
                                           exitPos.x, exitPos.y);
            }
        }
    }

    // --- Connection cables ---
    m_cachedConnectionPaths.clear();
    const auto connections = m_routingManager.getConnections();
    m_cachedConnectionPaths.reserve(connections.size());

    for (const auto& connection : connections)
    {
        const int srcIdx = connection.sourceSlot;
        const int dstIdx = connection.destinationSlot;

        if (srcIdx < 0 || srcIdx >= PedalSlotCount || dstIdx < 0 || dstIdx >= PedalSlotCount)
            continue;

        const auto p1 = m_pedalComponents[static_cast<size_t>(srcIdx)]->getOutputJackPos();
        const auto p2 = m_pedalComponents[static_cast<size_t>(dstIdx)]->getInputJackPos();
        const float horizontal = std::abs(p2.x - p1.x);
        const float vertical = std::abs(p2.y - p1.y);
        const float lift = 34.0f + horizontal * 0.08f + vertical * 0.10f;
        const float curve = std::max(horizontal * 0.34f, 46.0f);

        const juce::Point<float> cp1(p1.x + curve, p1.y - lift);
        const juce::Point<float> cp2(p2.x - curve, p2.y - lift);

        auto split = splitCubicBezier(p1, cp1, cp2, p2);
        m_cachedConnectionPaths.push_back({ std::move(split.left), std::move(split.right) });
    }
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
    for (const auto& cable : m_cachedConnectionPaths)
    {
        auto fullShadow = cable.left;
        fullShadow.addPath(cable.right);
        fullShadow.applyTransform(juce::AffineTransform::translation(3.0f, 7.0f));
        RenderUtils::strokeCable(g, fullShadow, juce::Colours::black.withAlpha(0.45f), 8.0f);

        RenderUtils::strokeCable(g, cable.left, m_theme.cableOutColour().darker(0.18f), 6.2f);
        RenderUtils::strokeCable(g, cable.left, m_theme.cableOutColour(), 4.8f);
        RenderUtils::strokeCable(g, cable.left, juce::Colours::white.withAlpha(0.14f), 1.4f);

        RenderUtils::strokeCable(g, cable.right, m_theme.cableInColour().darker(0.18f), 6.2f);
        RenderUtils::strokeCable(g, cable.right, m_theme.cableInColour(), 4.8f);
        RenderUtils::strokeCable(g, cable.right, juce::Colours::white.withAlpha(0.14f), 1.4f);
    }
}

void PedalboardGrid::drawActiveDraggingCable(juce::Graphics& g)
{
    if (!m_isDraggingCable)
        return;

    const auto p1 = m_dragStartPos;
    const auto p2 = m_dragCurrentPos;
    const float horizontal = std::abs(p2.x - p1.x);
    const float lift = 32.0f + horizontal * 0.06f;
    const float curve = std::max(horizontal * 0.34f, 44.0f);

    const juce::Point<float> cp1(p1.x + curve, p1.y - lift);
    const juce::Point<float> cp2(p2.x - curve, p2.y - lift);

    auto split = splitCubicBezier(p1, cp1, cp2, p2);

    RenderUtils::strokeCable(g, split.left, m_theme.cableOutColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.left, juce::Colours::white.withAlpha(0.16f), 1.2f);
    RenderUtils::strokeCable(g, split.right, m_theme.cableInColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, split.right, juce::Colours::white.withAlpha(0.16f), 1.2f);
}

void PedalboardGrid::drawInputCable(juce::Graphics& g)
{
    if (m_cachedInputPath.isEmpty())
        return;

    const auto& routing = m_routingManager.getRoutingOrder();
    if (routing.empty())
        return;

    const int firstSlot = routing.front();
    if (firstSlot < 0 || firstSlot >= PedalSlotCount)
        return;

    const auto* pedal = m_pedalComponents[static_cast<size_t>(firstSlot)].get();
    if (pedal == nullptr)
        return;

    const juce::Point<float> entryPos(static_cast<float>(getWidth()) * 0.05f, 0.0f);

    auto shadow = m_cachedInputPath;
    shadow.applyTransform(juce::AffineTransform::translation(3.0f, 7.0f));
    RenderUtils::strokeCable(g, shadow, juce::Colours::black.withAlpha(0.45f), 8.0f);
    RenderUtils::strokeCable(g, m_cachedInputPath, m_theme.cableInColour().darker(0.18f), 6.2f);
    RenderUtils::strokeCable(g, m_cachedInputPath, m_theme.cableInColour(), 4.8f);
    RenderUtils::strokeCable(g, m_cachedInputPath, juce::Colours::white.withAlpha(0.14f), 1.4f);

    constexpr float jackR = 7.0f;
    g.setColour(juce::Colours::dimgrey);
    g.fillEllipse(entryPos.x - jackR, entryPos.y - jackR, jackR * 2.0f, jackR * 2.0f);
    g.setColour(juce::Colours::silver);
    g.drawEllipse(entryPos.x - jackR, entryPos.y - jackR, jackR * 2.0f, jackR * 2.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("IN", entryPos.x + jackR + 4.0f, entryPos.y - 9.0f, 30.0f, 18.0f,
               juce::Justification::centredLeft, false);
}

void PedalboardGrid::drawOutputCable(juce::Graphics& g)
{
    if (m_cachedOutputPath.isEmpty())
        return;

    const auto& routing = m_routingManager.getRoutingOrder();
    if (routing.empty())
        return;

    const int lastSlot = routing.back();
    if (lastSlot < 0 || lastSlot >= PedalSlotCount)
        return;

    const auto* pedal = m_pedalComponents[static_cast<size_t>(lastSlot)].get();
    if (pedal == nullptr)
        return;

    const juce::Point<float> exitPos(static_cast<float>(getWidth()) * 0.95f, 0.0f);

    auto shadow = m_cachedOutputPath;
    shadow.applyTransform(juce::AffineTransform::translation(3.0f, 7.0f));
    RenderUtils::strokeCable(g, shadow, juce::Colours::black.withAlpha(0.45f), 8.0f);
    RenderUtils::strokeCable(g, m_cachedOutputPath, m_theme.cableOutColour().darker(0.18f), 6.2f);
    RenderUtils::strokeCable(g, m_cachedOutputPath, m_theme.cableOutColour(), 4.8f);
    RenderUtils::strokeCable(g, m_cachedOutputPath, juce::Colours::white.withAlpha(0.14f), 1.4f);

    constexpr float jackR = 7.0f;
    g.setColour(juce::Colours::dimgrey);
    g.fillEllipse(exitPos.x - jackR, exitPos.y - jackR, jackR * 2.0f, jackR * 2.0f);
    g.setColour(juce::Colours::silver);
    g.drawEllipse(exitPos.x - jackR, exitPos.y - jackR, jackR * 2.0f, jackR * 2.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText("OUT", exitPos.x - jackR - 30.0f - 4.0f, exitPos.y - 9.0f, 30.0f, 18.0f,
               juce::Justification::centredRight, false);
}

void PedalboardGrid::mouseDown(const juce::MouseEvent& event)
{
    const auto pos = event.getEventRelativeTo(this).position;
    const int jackIdx = findJackAt(pos, kJackRadius);

    if (jackIdx != -1)
    {
        m_isDraggingCable = true;
        m_dragSrcJackIdx = jackIdx;
        m_dragStartPos = m_cachedJacks[static_cast<size_t>(jackIdx)].pos;
        m_dragCurrentPos = pos;
        repaint();
    }
}

void PedalboardGrid::mouseDrag(const juce::MouseEvent& event)
{
    if (m_isDraggingCable)
    {
        m_dragCurrentPos = event.getEventRelativeTo(this).position;
        repaint();
    }
}

void PedalboardGrid::mouseUp(const juce::MouseEvent& event)
{
    if (!m_isDraggingCable)
        return;

    m_isDraggingCable = false;
    const int dstJackIdx = findJackAt(event.getEventRelativeTo(this).position, kJackRadius);

    if (dstJackIdx != -1 && dstJackIdx != m_dragSrcJackIdx)
    {
        auto& src = m_cachedJacks[static_cast<size_t>(m_dragSrcJackIdx)];
        auto& dst = m_cachedJacks[static_cast<size_t>(dstJackIdx)];

        if (!src.isInput && dst.isInput)
        {
            auto newRouting = m_routingManager.makeRoutingWithConnection(src.pedalIdx, dst.pedalIdx);
            m_routingManager.setManualRoutingOrder(newRouting);
            audioProcessor.setManualRouting(newRouting);
        }
    }

    repaint();
}

int PedalboardGrid::findJackAt(juce::Point<float> pos, float radius) const
{
    for (int i = 0; i < PedalSlotCount * 2; ++i)
        if (m_cachedJacks[static_cast<size_t>(i)].pos.getDistanceFrom(pos) <= radius)
            return i;

    return -1;
}
