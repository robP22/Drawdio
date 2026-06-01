#include "PluginEditor.h"

#include <cmath>
#include <utility>

namespace
{
juce::Colour darkerPanel() { return juce::Colour(0xFF151719); }
juce::Colour metalEdge() { return juce::Colour(0xFF3B4043); }

void fillVerticalGloss(juce::Graphics& g, juce::Rectangle<float> bounds,
                       juce::Colour top, juce::Colour bottom)
{
    juce::ColourGradient gradient(top, bounds.getX(), bounds.getY(),
                                  bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 8.0f);
}

void drawInsetPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 4.0f), radius);

    fillVerticalGloss(g, bounds, juce::Colour(0xFF252A2D), juce::Colour(0xFF111315));

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), radius - 1.0f, 1.0f);

    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawRoundedRectangle(bounds, radius, 2.0f);
}

juce::String colorName(PixelCanvasComponent::PixelColor color)
{
    switch (color)
    {
        case PixelCanvasComponent::PixelColor::White: return "WHITE";
        case PixelCanvasComponent::PixelColor::Red:   return "RED";
        case PixelCanvasComponent::PixelColor::Green: return "GREEN";
        case PixelCanvasComponent::PixelColor::Blue:  return "BLUE";
        case PixelCanvasComponent::PixelColor::Black:
        default:                                      return "BLACK";
    }
}
}

WorkspaceBackground::WorkspaceBackground()
{
    setInterceptsMouseClicks(false, false);
}

void WorkspaceBackground::paint(juce::Graphics& g)
{
    if (m_background.isValid())
        g.drawImage(m_background, getLocalBounds().toFloat());
    else
        g.fillAll(juce::Colour(0xFF20150D));
}

void WorkspaceBackground::resized()
{
    rebuildCachedBackground();
}

void WorkspaceBackground::rebuildCachedBackground()
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return;

    // Try to load texture from file first
    auto texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/workspace_background.png");

    if (texturePath.existsAsFile())
    {
        m_background = juce::ImageCache::getFromFile(texturePath);
        if (m_background.isValid())
        {
            // Resize texture to fit the background bounds
            m_background = m_background.rescaled(bounds.getWidth(),
                                                  bounds.getHeight(),
                                                  juce::Graphics::highResamplingQuality);
            return;
        }
    }

    // Fallback: Procedural wood texture
    m_background = juce::Image(juce::Image::RGB,
                               bounds.getWidth(),
                               bounds.getHeight(),
                               false);

    juce::Graphics bg(m_background);
    juce::ColourGradient base(juce::Colour(0xFF4A2E1C), 0.0f, 0.0f,
                              juce::Colour(0xFF17100B), 0.0f,
                              static_cast<float>(bounds.getHeight()), false);
    base.addColour(0.36, juce::Colour(0xFF302014));
    base.addColour(0.68, juce::Colour(0xFF24170F));
    bg.setGradientFill(base);
    bg.fillAll();

    juce::Random random(0x44726177);
    for (int i = 0; i < 850; ++i)
    {
        const float y = random.nextFloat() * static_cast<float>(bounds.getHeight());
        const float x = -40.0f + random.nextFloat() * 80.0f;
        const float length = static_cast<float>(bounds.getWidth()) + 120.0f;
        const float wobble = 10.0f + random.nextFloat() * 24.0f;

        juce::Path grain;
        grain.startNewSubPath(x, y);
        grain.cubicTo(length * 0.3f, y - wobble,
                      length * 0.62f, y + wobble,
                      length, y + random.nextFloat() * 18.0f - 9.0f);

        const auto alpha = 0.025f + random.nextFloat() * 0.05f;
        bg.setColour((i % 5 == 0 ? juce::Colour(0xFF8B5A35)
                                 : juce::Colour(0xFF0E0906)).withAlpha(alpha));
        bg.strokePath(grain, juce::PathStrokeType(0.7f + random.nextFloat() * 1.7f));
    }

    for (int i = 0; i < 36; ++i)
    {
        const float cx = random.nextFloat() * static_cast<float>(bounds.getWidth());
        const float cy = random.nextFloat() * static_cast<float>(bounds.getHeight());
        const float rw = 80.0f + random.nextFloat() * 180.0f;
        const float rh = 10.0f + random.nextFloat() * 26.0f;
        bg.setColour(juce::Colour(0xFF6C4529).withAlpha(0.045f));
        bg.fillEllipse(cx - rw * 0.5f, cy - rh * 0.5f, rw, rh);
    }

    juce::ColourGradient light(juce::Colours::white.withAlpha(0.13f), 0.0f, 0.0f,
                               juce::Colours::transparentWhite,
                               static_cast<float>(bounds.getWidth()) * 0.66f,
                               static_cast<float>(bounds.getHeight()) * 0.72f, true);
    bg.setGradientFill(light);
    bg.fillAll();

    juce::ColourGradient vignette(juce::Colours::transparentBlack,
                                  static_cast<float>(bounds.getCentreX()),
                                  static_cast<float>(bounds.getCentreY()),
                                  juce::Colours::black.withAlpha(0.58f),
                                  0.0f,
                                  static_cast<float>(bounds.getHeight()) * 0.68f,
                                  true);
    bg.setGradientFill(vignette);
    bg.fillAll();
}

ColorPalette::ColorPalette()
    : m_blobs {{
          { PixelCanvasComponent::PixelColor::Red,   {} },
          { PixelCanvasComponent::PixelColor::Green, {} },
          { PixelCanvasComponent::PixelColor::Blue,  {} },
          { PixelCanvasComponent::PixelColor::White, {} },
          { PixelCanvasComponent::PixelColor::Black, {} }
      }}
{
}

void ColorPalette::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Panel background with inset shadow
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRoundedRectangle(bounds.reduced(2.0f).translated(0.0f, 2.0f), 8.0f);

    // Metal panel
    juce::ColourGradient panelGrad(juce::Colour(0xFF2A2D2F), bounds.getX(), bounds.getY(),
                                    juce::Colour(0xFF141618), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(bounds.reduced(2.0f), 7.0f);

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const auto& blob = m_blobs[static_cast<size_t>(i)];
        const auto blobBounds = blob.bounds;
        const auto paintColour = PixelCanvasComponent::colourForPixel(blob.color);
        const bool selected = blob.color == m_selectedColor;
        const bool hovered = i == m_hoveredBlob;

        // Build splatter shape - irregular blob
        juce::Path splat;
        const auto cx = blobBounds.getCentreX();
        const auto cy = blobBounds.getCentreY();
        const auto rx = blobBounds.getWidth() * 0.5f;
        const auto ry = blobBounds.getHeight() * 0.5f;

        // Create irregular splatter with multiple lobes
        juce::Random random(static_cast<uint32_t>(i * 12345 + 42));
        juce::Point<float> points[8];
        for (int j = 0; j < 8; ++j)
        {
            float angle = static_cast<float>(j) * 0.7854f; // 45 degrees
            float radiusVar = 0.6f + random.nextFloat() * 0.5f;
            points[j] = juce::Point<float>(
                cx + std::cos(angle) * rx * radiusVar,
                cy + std::sin(angle) * ry * radiusVar);
        }

        // Draw main splatter blob
        splat.startNewSubPath(points[0]);
        for (int j = 1; j < 8; ++j)
        {
            // Cubic bezier to next point with random control points
            float midX = (points[j - 1].x + points[j].x) * 0.5f;
            float midY = (points[j - 1].y + points[j].y) * 0.5f;
            float cp1x = midX + (random.nextFloat() - 0.5f) * rx * 0.4f;
            float cp1y = midY + (random.nextFloat() - 0.5f) * ry * 0.4f;
            float cp2x = midX + (random.nextFloat() - 0.5f) * rx * 0.4f;
            float cp2y = midY + (random.nextFloat() - 0.5f) * ry * 0.4f;
            splat.cubicTo(cp1x, cp1y, cp2x, cp2y, points[j].x, points[j].y);
        }
        splat.closeSubPath();

        // Selection glow ring with elevation - use splatter shape
        if (selected)
        {
            // Outer glow
            g.setColour(paintColour.withAlpha(0.25f));
            g.fillPath(splat, juce::AffineTransform::scale(1.18f, 1.18f, cx, cy));

            // Selection ring
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.strokePath(splat, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Shadow
        const float shadowOffset = selected ? 4.0f : 7.0f;
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        juce::Path shadowSplat = splat;
        juce::AffineTransform shadowTrans = juce::AffineTransform::translation(0.0f, shadowOffset);
        g.fillPath(shadowSplat, shadowTrans);

        // Dark base
        g.setColour(paintColour.darker(0.35f));
        g.fillPath(splat);

        // Inner splatter (lighter center)
        auto innerBounds = blobBounds.reduced(rx * 0.3f, ry * 0.3f);
        juce::Path innerSplat;
        innerSplat.startNewSubPath(innerBounds.getX() + innerBounds.getWidth() * 0.2f, cy);
        innerSplat.cubicTo(innerBounds.getX(), cy - innerBounds.getHeight() * 0.3f,
                           innerBounds.getRight(), cy - innerBounds.getHeight() * 0.3f,
                           innerBounds.getRight() - innerBounds.getWidth() * 0.2f, cy);
        innerSplat.cubicTo(innerBounds.getRight(), cy + innerBounds.getHeight() * 0.3f,
                           innerBounds.getX(), cy + innerBounds.getHeight() * 0.3f,
                           innerBounds.getX() + innerBounds.getWidth() * 0.2f, cy);
        g.setColour(paintColour);
        g.fillPath(innerSplat);

        // Gloss highlight splat
        auto highlightBounds = blobBounds.reduced(rx * 0.6f, ry * 0.6f);
        g.setColour(juce::Colours::white.withAlpha(
            blob.color == PixelCanvasComponent::PixelColor::Black ? 0.2f : 0.4f));
        juce::Path highlight;
        highlight.startNewSubPath(highlightBounds.getX(), highlightBounds.getBottom());
        highlight.cubicTo(highlightBounds.getX(), highlightBounds.getY(),
                          highlightBounds.getRight(), highlightBounds.getY(),
                          highlightBounds.getRight(), highlightBounds.getBottom() - highlightBounds.getHeight() * 0.3f);
        highlight.cubicTo(highlightBounds.getRight() - highlightBounds.getWidth() * 0.3f, highlightBounds.getBottom(),
                          highlightBounds.getX() + highlightBounds.getWidth() * 0.3f, highlightBounds.getBottom(),
                          highlightBounds.getX(), highlightBounds.getBottom());
        highlight.closeSubPath();
        g.fillPath(highlight);

        // Hover effect
        if (hovered)
        {
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.strokePath(splat, juce::PathStrokeType(1.5f));
        }
    }

    // Panel border
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), 6.0f, 1.0f);
}

void ColorPalette::resized()
{
    auto area = getLocalBounds().reduced(8, 6).toFloat();
    const auto slotW = area.getWidth() / static_cast<float>(m_blobs.size());
    const auto blobSize = juce::jmin(52.0f, area.getHeight() - 4.0f);

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        auto slot = juce::Rectangle<float>(area.getX() + static_cast<float>(i) * slotW,
                                           area.getY(),
                                           slotW,
                                           area.getHeight());
        m_blobs[static_cast<size_t>(i)].bounds = slot.withSizeKeepingCentre(blobSize, blobSize * 0.78f);
    }
}

void ColorPalette::mouseDown(const juce::MouseEvent& event)
{
    const int index = hitTestBlob(event.position);
    if (index < 0)
        return;

    setSelectedColor(m_blobs[static_cast<size_t>(index)].color);

    if (m_onColorSelected)
        m_onColorSelected(m_selectedColor);
}

void ColorPalette::mouseMove(const juce::MouseEvent& event)
{
    const int hit = hitTestBlob(event.position);
    if (hit != m_hoveredBlob)
    {
        m_hoveredBlob = hit;
        setMouseCursor(hit >= 0 ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ColorPalette::mouseExit(const juce::MouseEvent&)
{
    m_hoveredBlob = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void ColorPalette::setSelectedColor(PixelCanvasComponent::PixelColor color)
{
    m_selectedColor = color;
    repaint();
}

int ColorPalette::hitTestBlob(juce::Point<float> position) const
{
    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
        if (m_blobs[static_cast<size_t>(i)].bounds.expanded(6.0f).contains(position))
            return i;

    return -1;
}

CanvasTools::CanvasTools()
{
    styleButton(m_undoButton, juce::Colour(0xFFC7B067));
    styleButton(m_clearButton, juce::Colour(0xFFD75B4F));

    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_clearButton);

    m_undoButton.onClick = [this]()
    {
        if (m_onUndo)
            m_onUndo();
    };

    m_clearButton.onClick = [this]()
    {
        if (m_onClear)
            m_onClear();
    };
}

void CanvasTools::paint(juce::Graphics& g)
{
    drawInsetPanel(g, getLocalBounds().toFloat().reduced(1.0f), 7.0f);
}

void CanvasTools::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    const auto buttonH = juce::jmax(24, (area.getHeight() - 4) / 2);
    m_undoButton.setBounds(area.removeFromTop(buttonH));
    area.removeFromTop(4);
    m_clearButton.setBounds(area.removeFromTop(buttonH));
}

void CanvasTools::styleButton(juce::TextButton& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF25292C));
    button.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.2f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

void CanvasStatusDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    drawInsetPanel(g, bounds, 7.0f);

    auto content = bounds.reduced(12.0f, 8.0f).toNearestInt();
    auto chip = content.removeFromLeft(34).reduced(0, 7).toFloat();

    // Draw splatter shape for active color
    juce::Path splat;
    const auto cx = chip.getCentreX();
    const auto cy = chip.getCentreY();
    const auto rx = chip.getWidth() * 0.5f;
    const auto ry = chip.getHeight() * 0.5f;

    juce::Random random(42);
    juce::Point<float> points[8];
    for (int j = 0; j < 8; ++j)
    {
        float angle = static_cast<float>(j) * 0.7854f;
        float radiusVar = 0.65f + random.nextFloat() * 0.45f;
        points[j] = juce::Point<float>(
            cx + std::cos(angle) * rx * radiusVar,
            cy + std::sin(angle) * ry * radiusVar);
    }

    splat.startNewSubPath(points[0]);
    for (int j = 1; j < 8; ++j)
    {
        float midX = (points[j - 1].x + points[j].x) * 0.5f;
        float midY = (points[j - 1].y + points[j].y) * 0.5f;
        float cp1x = midX + (random.nextFloat() - 0.5f) * rx * 0.35f;
        float cp1y = midY + (random.nextFloat() - 0.5f) * ry * 0.35f;
        float cp2x = midX + (random.nextFloat() - 0.5f) * rx * 0.35f;
        float cp2y = midY + (random.nextFloat() - 0.5f) * ry * 0.35f;
        splat.cubicTo(cp1x, cp1y, cp2x, cp2y, points[j].x, points[j].y);
    }
    splat.closeSubPath();

    g.setColour(PixelCanvasComponent::colourForPixel(m_selectedColor));
    g.fillPath(splat);
    g.setColour(juce::Colours::white.withAlpha(0.34f));
    g.strokePath(splat, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(juce::Colours::whitesmoke.withAlpha(0.88f));
    g.drawText(colorName(m_selectedColor), content.removeFromTop(22), juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colours::lightgrey.withAlpha(0.72f));
    g.drawText(juce::String(m_changedCellCount) + " / " + juce::String(TotalCells),
               content,
               juce::Justification::centredLeft);
}

void CanvasStatusDisplay::setSelectedColor(PixelCanvasComponent::PixelColor color)
{
    m_selectedColor = color;
    repaint();
}

void CanvasStatusDisplay::setChangedCellCount(int count)
{
    if (m_changedCellCount == count)
        return;

    m_changedCellCount = count;
    repaint();
}

CanvasModule::CanvasModule()
{
    addAndMakeVisible(m_pixelCanvas);
    addAndMakeVisible(m_palette);
    addAndMakeVisible(m_tools);

    m_palette.setOnColorSelected([this](auto color)
    {
        m_pixelCanvas.setCurrentColor(color);
    });

    m_tools.setOnUndo([this]()
    {
        m_pixelCanvas.undo();
        refreshStatus();
    });

    m_tools.setOnClear([this]()
    {
        if (m_onClear)
            m_onClear();

        m_pixelCanvas.clearCanvas();
        refreshStatus();
    });

    refreshStatus();
}

void CanvasModule::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Drop shadow beneath module
    g.setColour(juce::Colours::black.withAlpha(0.52f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 14.0f), 18.0f);

    // Dark industrial metal housing with visible thickness
    juce::ColourGradient housingGradient(juce::Colour(0xFF4A5054), bounds.getX(), bounds.getY(),
                                          juce::Colour(0xFF1A1D1F), bounds.getX(), bounds.getBottom(), false);
    housingGradient.addColour(0.3f, juce::Colour(0xFF3A3F43));
    housingGradient.addColour(0.7f, juce::Colour(0xFF25292C));
    g.setGradientFill(housingGradient);
    g.fillRoundedRectangle(bounds, 18.0f);

    // Metal edge highlight (upper-left)
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), 16.5f, 2.0f);

    // Inner bevel
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.reduced(2.5f), 15.5f, 1.5f);

    // Recessed canvas pocket - pure black for future texture background
    auto canvasPocket = m_pixelCanvas.getBounds().toFloat().expanded(12.0f);
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(canvasPocket, 12.0f);

    // Recessed inner highlight (upper-left soft lighting)
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(canvasPocket.reduced(1.5f), 10.5f, 1.0f);

    // Metal rivets/bolts at corners
    juce::Random rivetRandom(0xC0FFEE);
    float rivetRadius = 5.0f;
    juce::Point<float> rivetPositions[] = {
        { bounds.getX() + 16.0f, bounds.getY() + 16.0f },
        { bounds.getRight() - 16.0f, bounds.getY() + 16.0f },
        { bounds.getX() + 16.0f, bounds.getBottom() - 16.0f },
        { bounds.getRight() - 16.0f, bounds.getBottom() - 16.0f }
    };

    for (const auto& pos : rivetPositions)
    {
        // Rivet shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse(juce::Rectangle<float>(rivetRadius * 2.0f, rivetRadius * 2.0f)
                          .withCentre(pos.translated(1.0f, 2.0f)));

        // Rivet body
        juce::ColourGradient rivetGrad(juce::Colour(0xFF6A7074), pos.x - rivetRadius, pos.y - rivetRadius,
                                        juce::Colour(0xFF3A3F43), pos.x + rivetRadius, pos.y + rivetRadius, false);
        g.setGradientFill(rivetGrad);
        g.fillEllipse(juce::Rectangle<float>(rivetRadius * 2.0f, rivetRadius * 2.0f).withCentre(pos));

        // Rivet highlight
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillEllipse(juce::Rectangle<float>(rivetRadius, rivetRadius * 0.6f)
                          .withCentre(pos.translated(-rivetRadius * 0.3f, -rivetRadius * 0.3f)));
    }

    // Bottom edge shadow for depth
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRect(bounds.getX() + 20.0f, bounds.getBottom() - 8.0f, bounds.getWidth() - 40.0f, 6.0f);
}

void CanvasModule::resized()
{
    auto area = getLocalBounds().reduced(22, 20);
    auto bottom = area.removeFromBottom(82);
    area.removeFromBottom(14);

    const auto square = juce::jmin(area.getWidth(), area.getHeight());
    auto canvasArea = area.withSizeKeepingCentre(square, square);
    m_pixelCanvas.setBounds(canvasArea.reduced(8));

    auto controls = bottom;
    auto toolsArea = controls.removeFromRight(112);
    controls.removeFromRight(10);
    m_palette.setBounds(controls);
    m_tools.setBounds(toolsArea);
}

void CanvasModule::refreshStatus()
{
    m_changedCount = m_pixelCanvas.getChangedCellCount();
}

LevelMeter::LevelMeter(juce::String label)
    : m_label(std::move(label))
{
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawInsetPanel(g, bounds, 6.0f);

    auto meter = bounds.reduced(8.0f, 8.0f);
    auto labelArea = meter.removeFromLeft(26.0f);

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(juce::Colours::lightgrey.withAlpha(0.82f));
    g.drawText(m_label, labelArea.toNearestInt(), juce::Justification::centred);

    auto bar = meter.reduced(2.0f, 5.0f);
    g.setColour(juce::Colours::black.withAlpha(0.56f));
    g.fillRoundedRectangle(bar, 3.0f);

    auto lit = bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, m_level));
    juce::ColourGradient gradient(juce::Colour(0xFF36D987), lit.getX(), lit.getY(),
                                  juce::Colour(0xFFEBD45E), lit.getRight(), lit.getY(), false);
    gradient.addColour(0.82, juce::Colour(0xFFEBD45E));
    gradient.addColour(1.0, juce::Colour(0xFFE94D44));
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(lit, 3.0f);

    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawRoundedRectangle(bar, 3.0f, 1.0f);
}

void LevelMeter::setLevel(float level)
{
    level = juce::jlimit(0.0f, 1.0f, level);
    if (std::abs(level - m_level) < 0.002f)
        return;

    m_level = level;
    repaint();
}

BottomControlBar::BottomControlBar()
{
    addAndMakeVisible(m_inputMeter);
    addAndMakeVisible(m_outputMeter);
    addAndMakeVisible(m_dryWetSlider);
    addAndMakeVisible(m_oversamplingSelector);
    addAndMakeVisible(m_qualitySelector);

    m_dryWetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_dryWetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    m_dryWetSlider.setRange(0.0, 100.0, 1.0);
    m_dryWetSlider.setValue(50.0, juce::dontSendNotification);
    m_dryWetSlider.setTextValueSuffix("%");
    m_dryWetSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE6ECEF));
    m_dryWetSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF47C9A2));
    m_dryWetSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF0E1012));

    m_oversamplingSelector.addItem("1x", 1);
    m_oversamplingSelector.addItem("2x", 2);
    m_oversamplingSelector.addItem("4x", 3);
    m_oversamplingSelector.setSelectedId(1, juce::dontSendNotification);

    m_qualitySelector.addItem("Eco", 1);
    m_qualitySelector.addItem("Studio", 2);
    m_qualitySelector.addItem("Ultra", 3);
    m_qualitySelector.setSelectedId(2, juce::dontSendNotification);

    for (auto* combo : { &m_oversamplingSelector, &m_qualitySelector })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF1D2225));
        combo->setColour(juce::ComboBox::textColourId, juce::Colours::whitesmoke);
        combo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF4A555B));
        combo->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFB8C1C5));
    }
}

void BottomControlBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.fillRect(bounds);

    juce::ColourGradient gradient(juce::Colour(0xFF24282B), bounds.getX(), bounds.getY(),
                                  juce::Colour(0xFF0C0E10), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds.reduced(8.0f, 5.0f), 8.0f);

    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(bounds.reduced(9.0f, 6.0f), 7.0f, 1.0f);

    auto labels = getLocalBounds().reduced(24, 8);
    labels.removeFromLeft(414);
    auto dryLabel = labels.removeFromLeft(66);
    g.setColour(juce::Colours::lightgrey.withAlpha(0.76f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("DRY/WET", dryLabel, juce::Justification::centredLeft);

    labels.removeFromLeft(240);
    g.drawText("OS", labels.removeFromLeft(28), juce::Justification::centredLeft);
    labels.removeFromLeft(120);
    g.drawText("QUALITY", labels.removeFromLeft(58), juce::Justification::centredLeft);
}

void BottomControlBar::resized()
{
    auto area = getLocalBounds().reduced(24, 17);
    const int meterW = 190;
    m_inputMeter.setBounds(area.removeFromLeft(meterW));
    area.removeFromLeft(14);
    m_outputMeter.setBounds(area.removeFromLeft(meterW));
    area.removeFromLeft(28);

    area.removeFromLeft(70);
    m_dryWetSlider.setBounds(area.removeFromLeft(260).reduced(0, 4));
    area.removeFromLeft(34);

    area.removeFromLeft(30);
    m_oversamplingSelector.setBounds(area.removeFromLeft(116).reduced(0, 7));
    area.removeFromLeft(34);
    area.removeFromLeft(60);
    m_qualitySelector.setBounds(area.removeFromLeft(130).reduced(0, 7));
}

void BottomControlBar::setMeterLevels(float inputLevel, float outputLevel)
{
    m_inputMeter.setLevel(inputLevel);
    m_outputMeter.setLevel(outputLevel);
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      m_pedalboardCanvas(p)
{
    addAndMakeVisible(m_workspaceBackground);
    addAndMakeVisible(m_canvasModule);
    addAndMakeVisible(m_pedalboardCanvas);
    addAndMakeVisible(m_bottomControlBar);
    m_workspaceBackground.toBack();

    auto& pixelCanvas = m_canvasModule.getPixelCanvas();
    pixelCanvas.setGridData(audioProcessor.getGridData());
    pixelCanvas.setOnPenDown([this]()
    {
        audioProcessor.getPenDebouncer().penDown();
    });
    pixelCanvas.setOnPenUp([this]()
    {
        audioProcessor.getPenDebouncer().penUp();
    });
    pixelCanvas.setOnCanvasSnapshot([this](const auto&)
    {
        triggerRecompile();
        m_canvasModule.refreshStatus();
        juce::MessageManager::callAsync([this]() { timerCallback(); });
    });

    m_canvasModule.setOnClear([this]()
    {
        audioProcessor.setManualRouting({});
    });

    setSize(1400, 800);
    startTimerHz(30);
}

DrawdioProcessorEditor::~DrawdioProcessorEditor()
{
    stopTimer();
}

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(darkerPanel());
}

void DrawdioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    m_workspaceBackground.setBounds(bounds);

    auto bottom = bounds.removeFromBottom(78);
    m_bottomControlBar.setBounds(bottom);

    auto content = bounds.reduced(18, 16);
    const auto gap = 18;
    const auto pedalW = juce::jlimit(560, 620, content.getWidth() - 760);
    auto pedalArea = content.removeFromRight(pedalW);
    content.removeFromRight(gap);

    m_canvasModule.setBounds(content);
    m_pedalboardCanvas.setBounds(pedalArea);
}

void DrawdioProcessorEditor::triggerRecompile()
{
    const auto& grid = m_canvasModule.getPixelCanvas().getGridData();
    audioProcessor.getMessageQueue().pushSnapshot(grid.data());
    audioProcessor.setGridData(grid);
    audioProcessor.getCompilerThread().notify();
}

void DrawdioProcessorEditor::timerCallback()
{
    const auto previousRevision = m_seenConfigRevision;
    audioProcessor.consumeCompiledResultIfAvailable();
    m_seenConfigRevision = audioProcessor.getConfigRevision();
    const bool configChanged = m_seenConfigRevision != previousRevision;

    m_pedalboardCanvas.syncPedals();

    auto config = audioProcessor.getDSPProcessor().getCurrentConfig();
    if (config)
    {
        for (auto& param : config->parameters)
        {
            const int chainPos = static_cast<int>(param.targetDspNodeRegister);
            if (chainPos >= 0 && chainPos < static_cast<int>(config->routingSlotOrder.size()))
            {
                const int slotIdx = config->routingSlotOrder[static_cast<size_t>(chainPos)];
                if (auto* pedal = m_pedalboardCanvas.getPedal(slotIdx))
                    pedal->setKnobValue(static_cast<int>(param.parameterToken), param.currentValue);
            }
        }

        if (configChanged || config->routingSlotOrder != m_lastRoutingOrder)
        {
            m_lastRoutingOrder = config->routingSlotOrder;
            m_pedalboardCanvas.updateRouting(m_lastRoutingOrder);
        }
    }
    else if (!m_lastRoutingOrder.empty())
    {
        m_lastRoutingOrder.clear();
        m_pedalboardCanvas.updateRouting(m_lastRoutingOrder);
    }

    m_bottomControlBar.setMeterLevels(audioProcessor.getInputMeterLevel(),
                                      audioProcessor.getOutputMeterLevel());
    m_canvasModule.refreshStatus();
}
