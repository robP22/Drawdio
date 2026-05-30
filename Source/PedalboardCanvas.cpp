#include "PedalboardCanvas.h"
#include "PluginProcessor.h"

static const auto kCableColour     = juce::Colour(0xFF8B1A1A);
static constexpr float kShadowDx   = 2.0f;
static constexpr float kShadowDy   = 6.0f;
static constexpr float kJackRadius = 15.0f;

PedalboardCanvas::PedalboardCanvas(DrawdioProcessor& processor)
    : audioProcessor(processor)
{
    for (int s = 0; s < 6; ++s)
    {
        m_pedalComponents[s] = std::make_unique<PedalComponent>(
            audioProcessor, s, audioProcessor.getPedalSlot(s));
        addAndMakeVisible(m_pedalComponents[s].get());
    }
}

void PedalboardCanvas::paint(juce::Graphics& g)
{
    g.drawImageAt(m_feltImage, 0, 0);

    // Draw pedal drop shadows (behind cables)
    for (auto& pedal : m_pedalComponents)
    {
        if (!pedal) continue;
        auto pb = pedal->getBounds().toFloat();
        juce::Path shadow;
        shadow.addRoundedRectangle(pb.translated(kShadowDx, kShadowDy), 12.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillPath(shadow);
    }

    drawRoutingCables(g);
    drawActiveDraggingCable(g);
}

void PedalboardCanvas::resized()
{
    auto bounds = getLocalBounds();

    // Cache felt background
    m_feltImage = juce::Image(juce::Image::RGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics ig(m_feltImage);
    
    juce::ColourGradient feltGrad(juce::Colour(0xFF1A1A1A), 0, 0,
                                   juce::Colours::black, 0,
                                   static_cast<float>(bounds.getHeight()), false);
    ig.setGradientFill(feltGrad);
    ig.fillAll();

    auto& r = juce::Random::getSystemRandom();
    for (int i = 0; i < 2000; ++i)
    {
        ig.setColour(juce::Colours::white.withAlpha(0.02f));
        ig.fillRect(r.nextInt(bounds.getWidth()), r.nextInt(bounds.getHeight()), 1, 1);
    }

    int colW = bounds.getWidth() / 3;
    int rowH = bounds.getHeight() / 2;

    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = juce::Rectangle<int>(0, row * rowH, bounds.getWidth(), rowH);
        for (int col = 0; col < 3; ++col)
        {
            int s = row * 3 + col;
            auto slotBounds = rowArea.removeFromLeft(colW).reduced(12, 16);
            m_pedalComponents[static_cast<size_t>(s)]->setBounds(slotBounds);
        }
    }
}

void PedalboardCanvas::updateRouting(const std::vector<uint8_t>& routingOrder)
{
    m_routingOrder = routingOrder;
    repaint();
}

void PedalboardCanvas::syncPedals()
{
    for (auto& pedal : m_pedalComponents)
        pedal->syncFromProcessor();
}

void PedalboardCanvas::drawRoutingCables(juce::Graphics& g)
{
    if (m_routingOrder.size() < 2) return;

    for (size_t i = 0; i < m_routingOrder.size() - 1; ++i)
    {
        int srcIdx = m_routingOrder[i];
        int dstIdx = m_routingOrder[i + 1];

        if (srcIdx < 0 || srcIdx >= 6 || dstIdx < 0 || dstIdx >= 6) continue;

        auto p1 = m_pedalComponents[static_cast<size_t>(srcIdx)]->getOutputJackPos();
        auto p2 = m_pedalComponents[static_cast<size_t>(dstIdx)]->getInputJackPos();

        float dx = std::max(std::abs(p2.x - p1.x) * 0.4f, 30.0f);

        juce::Path path;
        path.startNewSubPath(p1);
        path.cubicTo(p1.x + dx, p1.y, p2.x - dx, p2.y, p2.x, p2.y);

        // Shadow
        juce::Path shadowPath = path;
        shadowPath.applyTransform(juce::AffineTransform::translation(kShadowDx, kShadowDy));
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.strokePath(shadowPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Main cable
        g.setColour(kCableColour);
        g.strokePath(path, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Glossy highlight
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.strokePath(path, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void PedalboardCanvas::drawActiveDraggingCable(juce::Graphics& g)
{
    if (!m_isDraggingCable) return;

    auto p1 = m_dragStartPos;
    auto p2 = m_dragCurrentPos;

    float dx = std::max(std::abs(p2.x - p1.x) * 0.4f, 30.0f);
    juce::Path path;
    path.startNewSubPath(p1);
    path.cubicTo(p1.x + dx, p1.y, p2.x - dx, p2.y, p2.x, p2.y);

    g.setColour(kCableColour.withAlpha(0.6f));
    g.strokePath(path, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PedalboardCanvas::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;
    int jackIdx = findJackAt(pos, kJackRadius);
    
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
    if (m_isDraggingCable)
    {
        m_isDraggingCable = false;
        int dstJackIdx = findJackAt(event.position, kJackRadius);
        
        if (dstJackIdx != -1 && dstJackIdx != m_dragSrcJackIdx)
        {
            auto jacks = getJacks();
            auto src = jacks[static_cast<size_t>(m_dragSrcJackIdx)];
            auto dst = jacks[static_cast<size_t>(dstJackIdx)];
            
            // Logic: Connect Output to Input
            if (!src.isInput && dst.isInput)
            {
                auto newRouting = m_routingOrder;
                
                // If src pedal is in the routing, try to connect it to dst pedal
                auto it = std::find(newRouting.begin(), newRouting.end(), static_cast<uint8_t>(src.pedalIdx));
                if (it != newRouting.end())
                {
                    // Remove dst pedal if it's already in the routing elsewhere
                    newRouting.erase(std::remove(newRouting.begin(), newRouting.end(), static_cast<uint8_t>(dst.pedalIdx)), newRouting.end());
                    
                    // Insert dst pedal after src pedal
                    newRouting.insert(it + 1, static_cast<uint8_t>(dst.pedalIdx));
                }
                else
                {
                    // If src pedal not in routing, start a new one or append
                    if (newRouting.empty()) newRouting.push_back(static_cast<uint8_t>(src.pedalIdx));
                    newRouting.push_back(static_cast<uint8_t>(dst.pedalIdx));
                }
                
                audioProcessor.setManualRouting(newRouting);
            }
        }
        repaint();
    }
}

std::vector<PedalboardCanvas::JackInfo> PedalboardCanvas::getJacks() const
{
    std::vector<JackInfo> jacks;
    for (int i = 0; i < 6; ++i)
    {
        jacks.push_back({i, true, m_pedalComponents[static_cast<size_t>(i)]->getInputJackPos()});
        jacks.push_back({i, false, m_pedalComponents[static_cast<size_t>(i)]->getOutputJackPos()});
    }
    return jacks;
}

int PedalboardCanvas::findJackAt(juce::Point<float> pos, float radius) const
{
    auto jacks = getJacks();
    for (int i = 0; i < static_cast<int>(jacks.size()); ++i)
    {
        if (jacks[static_cast<size_t>(i)].pos.getDistanceFrom(pos) <= radius)
            return i;
    }
    return -1;
}
