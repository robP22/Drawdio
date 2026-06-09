#include "PedalboardGrid.h"
#include "GridLayout.h"
#include "PluginProcessor.h"
#include "RenderUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kJackRadius = 16.0f;
constexpr int kPedalboardColumns = 3;
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
    for (int s = 0; s < PedalSlotCount; ++s)
    {
        m_pedalComponents[static_cast<size_t>(s)] = std::make_unique<PedalComponent>(
            audioProcessor, s, audioProcessor.getPedalSlot(s), m_resources, m_theme);
        addAndMakeVisible(m_pedalComponents[static_cast<size_t>(s)].get());
    }
}

void PedalboardGrid::paint(juce::Graphics&)
{
    // COMMENTED OUT FOR DEBUG - grid rendering disabled
}

void PedalboardGrid::resized()
{
    auto bounds = getLocalBounds().withTrimmedLeft(GridLayout::GridSidePadding)
                                        .withTrimmedRight(GridLayout::GridSidePadding)
                                        .withTrimmedTop(GridLayout::GridSidePadding)
                                        .withTrimmedBottom(GridLayout::GridSidePadding);

    // Determine pedal size from available grid space, then scale down 5%.
    const int pedalW = juce::roundToInt(juce::jlimit(
        GridLayout::PedalWidthMin,
        GridLayout::PedalWidthMax,
        bounds.getWidth() / GridLayout::ColCount - 16) * 0.95f);
    const int pedalH = juce::roundToInt(juce::jlimit(
        GridLayout::PedalHeightMin,
        GridLayout::PedalHeightMax,
        bounds.getHeight() / GridLayout::RowCount - 16) * 0.95f);

    // Inter-pedal x gap is fixed; the pedal group is centred horizontally.
    constexpr int xInnerGap = 7;
    const int groupW  = pedalW * GridLayout::ColCount + xInnerGap * (GridLayout::ColCount - 1);
    const int xOrigin = bounds.getX() + (bounds.getWidth() - groupW) / 2;

    // Inter-row y gap is fixed; the pedal group is centred vertically then
    // shifted down by VerticalGroupOffset fraction of the total grid height.
    constexpr float VerticalGroupOffset = 0.0375f;  // tune to shift group down; 0 = centred
    constexpr int yInnerGap = 70;
    const int groupH  = pedalH * GridLayout::RowCount + yInnerGap * (GridLayout::RowCount - 1);
    const int yOrigin = bounds.getY() + (bounds.getHeight() - groupH) / 2
                        + juce::roundToInt(bounds.getHeight() * VerticalGroupOffset);

    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        const int row = slot / GridLayout::ColCount;
        const int col = slot % GridLayout::ColCount;

        const int x = xOrigin + col * (pedalW + xInnerGap);
        const int y = yOrigin + row * (pedalH + yInnerGap);

        m_pedalComponents[static_cast<size_t>(slot)]->setBounds(x, y, pedalW, pedalH);
    }
}

void PedalboardGrid::updateRouting(const std::vector<uint8_t>& routingOrder)
{
    m_routingManager.setRoutingOrder(routingOrder);
    repaint();
}

void PedalboardGrid::syncPedals()
{
    for (auto& pedal : m_pedalComponents)
        if (pedal)
            pedal->syncFromProcessor();
}

void PedalboardGrid::drawRoutingCables(juce::Graphics& g)
{
    const auto connections = m_routingManager.getConnections();
    if (connections.empty())
        return;

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

        juce::Path path;
        path.startNewSubPath(p1);
        path.cubicTo(p1.x + curve, p1.y - lift,
                     p2.x - curve, p2.y - lift,
                     p2.x, p2.y);

        auto shadow = path;
        shadow.applyTransform(juce::AffineTransform::translation(3.0f, 7.0f));
        RenderUtils::strokeCable(g, shadow, juce::Colours::black.withAlpha(0.45f), 8.0f);
        RenderUtils::strokeCable(g, path, m_theme.cableColour().darker(0.18f), 6.2f);
        RenderUtils::strokeCable(g, path, m_theme.cableColour(), 4.8f);
        RenderUtils::strokeCable(g, path, juce::Colours::white.withAlpha(0.14f), 1.4f);
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

    juce::Path path;
    path.startNewSubPath(p1);
    path.cubicTo(p1.x + curve, p1.y - lift,
                 p2.x - curve, p2.y - lift,
                 p2.x, p2.y);

    RenderUtils::strokeCable(g, path, m_theme.cableColour().withAlpha(0.66f), 4.4f);
    RenderUtils::strokeCable(g, path, juce::Colours::white.withAlpha(0.16f), 1.2f);
}

void PedalboardGrid::mouseDown(const juce::MouseEvent& event)
{
    const auto pos = event.position;
    const int jackIdx = findJackAt(pos, kJackRadius);

    if (jackIdx != -1)
    {
        auto jacks = getJacks();
        m_isDraggingCable = true;
        m_dragSrcJackIdx = jackIdx;
        m_dragStartPos = jacks[static_cast<size_t>(jackIdx)].pos;
        m_dragCurrentPos = pos;
        repaint();
    }
}

void PedalboardGrid::mouseDrag(const juce::MouseEvent& event)
{
    if (m_isDraggingCable)
    {
        m_dragCurrentPos = event.position;
        repaint();
    }
}

void PedalboardGrid::mouseUp(const juce::MouseEvent& event)
{
    if (!m_isDraggingCable)
        return;

    m_isDraggingCable = false;
    const int dstJackIdx = findJackAt(event.position, kJackRadius);

    if (dstJackIdx != -1 && dstJackIdx != m_dragSrcJackIdx)
    {
        auto jacks = getJacks();
        auto src = jacks[static_cast<size_t>(m_dragSrcJackIdx)];
        auto dst = jacks[static_cast<size_t>(dstJackIdx)];

        if (!src.isInput && dst.isInput)
        {
            auto newRouting = m_routingManager.makeRoutingWithConnection(src.pedalIdx, dst.pedalIdx);
            m_routingManager.setManualRoutingOrder(newRouting);
            audioProcessor.setManualRouting(newRouting);
        }
    }

    repaint();
}

std::vector<PedalboardGrid::JackInfo> PedalboardGrid::getJacks() const
{
    std::vector<JackInfo> jacks;
    jacks.reserve(PedalSlotCount * 2);

    for (int i = 0; i < PedalSlotCount; ++i)
    {
        jacks.push_back({ i, true, m_pedalComponents[static_cast<size_t>(i)]->getInputJackPos() });
        jacks.push_back({ i, false, m_pedalComponents[static_cast<size_t>(i)]->getOutputJackPos() });
    }

    return jacks;
}

int PedalboardGrid::findJackAt(juce::Point<float> pos, float radius) const
{
    auto jacks = getJacks();
    for (int i = 0; i < static_cast<int>(jacks.size()); ++i)
        if (jacks[static_cast<size_t>(i)].pos.getDistanceFrom(pos) <= radius)
            return i;

    return -1;
}
