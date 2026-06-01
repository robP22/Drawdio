#include "PedalboardCanvas.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kJackRadius = 16.0f;
constexpr float kShadowDx = 2.0f;
constexpr float kShadowDy = 6.0f;
const auto kCableColour = juce::Colour(0xFF7E2020);

void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(thickness,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}
}

PedalboardCanvas::PedalboardCanvas(DrawdioProcessor& processor)
    : audioProcessor(processor)
{
    for (int s = 0; s < 6; ++s)
    {
        m_pedalComponents[static_cast<size_t>(s)] = std::make_unique<PedalComponent>(
            audioProcessor, s, audioProcessor.getPedalSlot(s));
        addAndMakeVisible(m_pedalComponents[static_cast<size_t>(s)].get());
    }
}

void PedalboardCanvas::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.48f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 9.0f), 14.0f);

    juce::ColourGradient frameGradient(juce::Colour(0xFF5A3822), bounds.getX(), bounds.getY(),
                                       juce::Colour(0xFF1D120B), bounds.getX(), bounds.getBottom(), false);
    frameGradient.addColour(0.48, juce::Colour(0xFF3B2415));
    g.setGradientFill(frameGradient);
    g.fillRoundedRectangle(bounds, 14.0f);

    juce::Random grainRandom(0xB04D);
    for (int i = 0; i < 180; ++i)
    {
        const float y = bounds.getY() + grainRandom.nextFloat() * bounds.getHeight();
        g.setColour((i % 4 == 0 ? juce::Colour(0xFF8B5835) : juce::Colour(0xFF130C08)).withAlpha(0.06f));
        g.drawLine(bounds.getX() + 8.0f,
                   y,
                   bounds.getRight() - 8.0f,
                   y + grainRandom.nextFloat() * 8.0f - 4.0f,
                   0.8f + grainRandom.nextFloat());
    }

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 13.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds, 14.0f, 2.0f);

    if (m_feltImage.isValid())
        g.drawImageAt(m_feltImage, m_feltBounds.getX(), m_feltBounds.getY());
    else
        g.fillRoundedRectangle(m_feltBounds.toFloat(), 9.0f);

    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.drawRoundedRectangle(m_feltBounds.toFloat().expanded(2.0f), 10.0f, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(m_feltBounds.toFloat().reduced(1.0f), 8.0f, 1.0f);

    for (const auto& pedal : m_pedalComponents)
    {
        if (!pedal)
            continue;

        auto pedalBounds = pedal->getBounds().toFloat();
        juce::Path shadow;
        shadow.addRoundedRectangle(pedalBounds.translated(kShadowDx, kShadowDy), 14.0f);
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillPath(shadow);
    }

    drawRoutingCables(g);
    drawActiveDraggingCable(g);
}

void PedalboardCanvas::resized()
{
    auto bounds = getLocalBounds();
    m_boardBounds = bounds.reduced(2);
    m_feltBounds = m_boardBounds.reduced(21, 19);
    rebuildFeltImage();

    const int colW = m_feltBounds.getWidth() / 3;
    const int rowH = m_feltBounds.getHeight() / 2;
    const int pedalW = juce::jlimit(142, 180, colW - 20);
    const int pedalH = juce::jlimit(194, 240, rowH - 26);

    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            const int slot = row * 3 + col;
            auto slotBounds = juce::Rectangle<int>(m_feltBounds.getX() + col * colW,
                                                   m_feltBounds.getY() + row * rowH,
                                                   colW,
                                                   rowH).reduced(6, 8);
            auto pedalBounds = slotBounds.withSizeKeepingCentre(pedalW, pedalH);
            m_pedalComponents[static_cast<size_t>(slot)]->setBounds(pedalBounds);
        }
    }
}

void PedalboardCanvas::rebuildFeltImage()
{
    if (m_feltBounds.isEmpty())
        return;

    // Try to load texture from plugin bundle
    auto assetDir = juce::File::getSpecialLocation(juce::File::invokedExecutableFile).getParentDirectory();
    auto texturePath = assetDir.getChildFile("Contents/Resources/Assets/Textures/pedalboard_bg.png");

    if (!texturePath.existsAsFile())
    {
        // Fallback for development builds
        texturePath = juce::File::getCurrentWorkingDirectory().getChildFile("Assets/Textures/pedalboard_bg.png");
    }

    if (texturePath.existsAsFile())
    {
        m_feltImage = juce::ImageCache::getFromFile(texturePath);
        if (m_feltImage.isValid())
        {
            // Resize texture to fit the felt bounds
            m_feltImage = m_feltImage.rescaled(m_feltBounds.getWidth(),
                                               m_feltBounds.getHeight(),
                                               juce::Graphics::highResamplingQuality);
            return;
        }
    }

    // Fallback: Procedural felt texture
    m_feltImage = juce::Image(juce::Image::RGB,
                              m_feltBounds.getWidth(),
                              m_feltBounds.getHeight(),
                              false);
    juce::Graphics felt(m_feltImage);
    juce::ColourGradient feltGradient(juce::Colour(0xFF24292A), 0.0f, 0.0f,
                                      juce::Colour(0xFF070909), 0.0f,
                                      static_cast<float>(m_feltBounds.getHeight()),
                                      false);
    feltGradient.addColour(0.45, juce::Colour(0xFF151A1A));
    felt.setGradientFill(feltGradient);
    felt.fillAll();

    juce::Random random(0xF317);
    for (int i = 0; i < 3800; ++i)
    {
        const int x = random.nextInt(m_feltBounds.getWidth());
        const int y = random.nextInt(m_feltBounds.getHeight());
        felt.setColour((i % 3 == 0 ? juce::Colours::white : juce::Colours::black)
                           .withAlpha(0.018f + random.nextFloat() * 0.028f));
        felt.fillRect(x, y, 1, 1);
    }

    for (int y = 0; y < m_feltBounds.getHeight(); y += 4)
    {
        felt.setColour(juce::Colours::white.withAlpha(0.012f));
        felt.drawHorizontalLine(y, 0.0f, static_cast<float>(m_feltBounds.getWidth()));
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
        if (pedal)
            pedal->syncFromProcessor();
}

void PedalboardCanvas::drawRoutingCables(juce::Graphics& g)
{
    if (m_routingOrder.size() < 2)
        return;

    for (size_t i = 0; i + 1 < m_routingOrder.size(); ++i)
    {
        const int srcIdx = m_routingOrder[i];
        const int dstIdx = m_routingOrder[i + 1];

        if (srcIdx < 0 || srcIdx >= 6 || dstIdx < 0 || dstIdx >= 6)
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

        // Anti-aliased shadow layer
        auto shadow = path;
        shadow.applyTransform(juce::AffineTransform::translation(4.0f, 8.0f));
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.strokePath(shadow, juce::PathStrokeType(12.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Secondary shadow for depth
        auto shadow2 = path;
        shadow2.applyTransform(juce::AffineTransform::translation(2.0f, 4.0f));
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.strokePath(shadow2, juce::PathStrokeType(8.0f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // Cable body - darker base
        g.setColour(kCableColour.darker(0.25f));
        g.strokePath(path, juce::PathStrokeType(6.4f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Cable main color
        g.setColour(kCableColour);
        g.strokePath(path, juce::PathStrokeType(5.0f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Cable highlight for 3D effect
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.strokePath(path, juce::PathStrokeType(1.6f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Connector plugs at jacks
        drawCablePlug(g, p1, false);
        drawCablePlug(g, p2, true);
    }
}

void PedalboardCanvas::drawCablePlug(juce::Graphics& g, juce::Point<float> pos, bool isInput)
{
    // Plug shadow
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillEllipse(juce::Rectangle<float>(10.0f, 6.0f).withCentre(pos.translated(1.0f, 2.0f)));

    // Plug body
    juce::ColourGradient plugGrad(juce::Colour(0xFF8B8B8B),
                                   pos.x - 4.0f, pos.y - 3.0f,
                                   juce::Colour(0xFF3A3A3A),
                                   pos.x + 4.0f, pos.y + 3.0f,
                                   false);
    g.setGradientFill(plugGrad);
    g.fillEllipse(juce::Rectangle<float>(8.0f, 4.0f).withCentre(pos));

    // Plug highlight
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.fillEllipse(juce::Rectangle<float>(3.0f, 2.0f)
                      .withCentre(pos.translated(-1.0f, -1.0f)));
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

    strokeCable(g, path, kCableColour.withAlpha(0.66f), 4.4f);
    strokeCable(g, path, juce::Colours::white.withAlpha(0.16f), 1.2f);
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
            auto newRouting = m_routingOrder;

            if (std::find(newRouting.begin(), newRouting.end(), static_cast<uint8_t>(src.pedalIdx))
                == newRouting.end())
            {
                newRouting.push_back(static_cast<uint8_t>(src.pedalIdx));
            }

            newRouting.erase(std::remove(newRouting.begin(), newRouting.end(),
                                         static_cast<uint8_t>(dst.pedalIdx)),
                             newRouting.end());

            auto srcIt = std::find(newRouting.begin(), newRouting.end(),
                                   static_cast<uint8_t>(src.pedalIdx));
            if (srcIt != newRouting.end())
                newRouting.insert(srcIt + 1, static_cast<uint8_t>(dst.pedalIdx));

            audioProcessor.setManualRouting(newRouting);
        }
    }

    repaint();
}

std::vector<PedalboardCanvas::JackInfo> PedalboardCanvas::getJacks() const
{
    std::vector<JackInfo> jacks;
    jacks.reserve(12);

    for (int i = 0; i < 6; ++i)
    {
        jacks.push_back({ i, true, m_pedalComponents[static_cast<size_t>(i)]->getInputJackPos() });
        jacks.push_back({ i, false, m_pedalComponents[static_cast<size_t>(i)]->getOutputJackPos() });
    }

    return jacks;
}

int PedalboardCanvas::findJackAt(juce::Point<float> pos, float radius) const
{
    for (int i = 0; i < 12; ++i)
    {
        bool isInput = (i % 2 == 0);
        int pedalIdx = i / 2;
        auto jackPos = isInput ? m_pedalComponents[static_cast<size_t>(pedalIdx)]->getInputJackPos()
                               : m_pedalComponents[static_cast<size_t>(pedalIdx)]->getOutputJackPos();
        if (jackPos.getDistanceFrom(pos) <= radius)
            return i;
    }
    return -1;
}
