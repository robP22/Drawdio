#include "PluginEditor.h"

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
    // Transparent background - show wood texture
    auto bounds = getLocalBounds().toFloat();

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
    // Transparent - show wood background
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
    });

    m_tools.setOnClear([this]()
    {
        if (m_onClear)
            m_onClear();

        m_pixelCanvas.clearCanvas();
    });
}

void CanvasModule::paint(juce::Graphics& g)
{
    // Transparent - show wood background underneath
}

void CanvasModule::resized()
{
    auto area = getLocalBounds().reduced(12, 10);
    auto bottom = area.removeFromBottom(70);
    area.removeFromBottom(10);

    const auto square = juce::jmin(area.getWidth(), area.getHeight());
    auto canvasArea = area.withSizeKeepingCentre(square, square);
    m_pixelCanvas.setBounds(canvasArea.reduced(4));

    auto controls = bottom;
    auto toolsArea = controls.removeFromRight(100);
    controls.removeFromRight(8);
    m_palette.setBounds(controls);
    m_tools.setBounds(toolsArea);
}

DrawdioProcessorEditor::DrawdioProcessorEditor(DrawdioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      m_pedalboardCanvas(p)
{
    // Load wood texture
    auto texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/wood_texture_generic.png");
    if (texturePath.existsAsFile())
    {
        m_woodBackground = juce::ImageCache::getFromFile(texturePath);
    }

    addAndMakeVisible(m_canvasModule);
    addAndMakeVisible(m_pedalboardCanvas);

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
    });

    m_canvasModule.setOnClear([this]()
    {
        audioProcessor.setManualRouting({});
    });

    setSize(1400, 800);
}

DrawdioProcessorEditor::~DrawdioProcessorEditor() = default;

void DrawdioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (m_woodBackground.isValid())
    {
        g.drawImage(m_woodBackground, bounds);
    }
    else
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
    }
}

void DrawdioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Resize wood background if needed
    if (m_woodBackground.isValid())
    {
        m_woodBackground = m_woodBackground.rescaled(bounds.getWidth(),
                                                     bounds.getHeight(),
                                                     juce::Graphics::highResamplingQuality);
    }

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
