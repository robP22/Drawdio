#include "PedalboardCanvas.h"
#include "PluginProcessor.h"
#include "RenderUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kJackRadius = 16.0f;
constexpr int kPedalboardColumns = 3;
}

PedalboardCanvas::PedalboardCanvas(DrawdioProcessor& processor,
                                   const ResourceManager& resources,
                                   const ThemeManager& theme,
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

void PedalboardCanvas::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 9.0f), 14.0f);

    RenderUtils::drawTextureClippedToRoundedRect(g,
                                                 m_resources.getTexture(ResourceManager::TextureId::WorkspaceWood),
                                                 bounds,
                                                 14.0f,
                                                 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 13.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds, 14.0f, 2.0f);

    RenderUtils::drawTextureClippedToRoundedRect(g,
                                                 m_resources.getTexture(ResourceManager::TextureId::PedalboardFelt),
                                                 m_feltBounds.toFloat(),
                                                 9.0f,
                                                 1.0f);

    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.drawRoundedRectangle(m_feltBounds.toFloat().expanded(2.0f), 10.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(m_feltBounds.toFloat().reduced(1.0f), 8.0f, 1.0f);

    for (const auto& pedal : m_pedalComponents)
    {
        if (!pedal)
            continue;

        const auto pedalBounds = pedal->getBounds().toFloat();
        const auto& style = m_theme.pedalStyle();
        RenderUtils::drawSoftShadow(g,
                                    pedalBounds.translated(style.shadowOffsetX, style.shadowOffsetY),
                                    style.bodyRadius + 1.0f,
                                    style.shadowAlpha);
    }

    drawRoutingCables(g);
    drawActiveDraggingCable(g);
}

void PedalboardCanvas::resized()
{
    auto bounds = getLocalBounds();
    m_boardBounds = bounds.reduced(2);
    m_feltBounds = m_boardBounds.reduced(21, 19);

    const int rowCount = (PedalSlotCount + kPedalboardColumns - 1) / kPedalboardColumns;
    const int colW = m_feltBounds.getWidth() / kPedalboardColumns;
    const int rowH = m_feltBounds.getHeight() / rowCount;
    const int pedalW = juce::jlimit(142, 180, colW - 20);
    const int pedalH = juce::jlimit(194, 240, rowH - 26);

    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        const int row = slot / kPedalboardColumns;
        const int col = slot % kPedalboardColumns;
        auto slotBounds = juce::Rectangle<int>(m_feltBounds.getX() + col * colW,
                                               m_feltBounds.getY() + row * rowH,
                                               colW,
                                               rowH).reduced(6, 8);
        auto pedalBounds = slotBounds.withSizeKeepingCentre(pedalW, pedalH);
        m_pedalComponents[static_cast<size_t>(slot)]->setBounds(pedalBounds);
    }
}

void PedalboardCanvas::updateRouting(const std::vector<uint8_t>& routingOrder)
{
    m_routingManager.setRoutingOrder(routingOrder);
    repaint();
}

void PedalboardCanvas::syncPedals()
{
    for (auto& pedal : m_pedalComponents)
        if (pedal)
            pedal->syncFromProcessor();
}

void PedalboardCanvas::drawRoutingCables(juce::Graphics& g)
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

void PedalboardCanvas::drawActiveDraggingCable(juce::Graphics& g)
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

void PedalboardCanvas::mouseDown(const juce::MouseEvent& event)
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

void PedalboardCanvas::mouseDrag(const juce::MouseEvent& event)
{
    if (m_isDraggingCable)
    {
        m_dragCurrentPos = event.position;
        repaint();
    }
}

void PedalboardCanvas::mouseUp(const juce::MouseEvent& event)
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

std::vector<PedalboardCanvas::JackInfo> PedalboardCanvas::getJacks() const
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

int PedalboardCanvas::findJackAt(juce::Point<float> pos, float radius) const
{
    auto jacks = getJacks();
    for (int i = 0; i < static_cast<int>(jacks.size()); ++i)
        if (jacks[static_cast<size_t>(i)].pos.getDistanceFrom(pos) <= radius)
            return i;

    return -1;
}
