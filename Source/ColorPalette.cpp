#include "ColorPalette.h"
#include "PixelCanvasComponent.h"
#include "RenderUtils.h"
#include "ResourceManager.h"
#include "ThemeManager.h"

namespace Layout
{
    constexpr int PaletteLeftMargin   = 30;
    constexpr int PaletteRightMargin  = 30;
    constexpr int PaletteTopMargin    = 20;
    constexpr int PaletteBottomMargin = 35;
    constexpr float PaletteBlobShift  = 10.0f;
    constexpr int PaletteButtonRightPadding = 8;
    
    // Button dimensions
    constexpr float BlobSpacing = 10.5f;
    constexpr float BlobMaxSize = 72.0f;
    constexpr int ButtonWidth = 38;
    constexpr int ButtonHeight = 38;
    constexpr int ButtonSpacing = 6;
}

ColorPalette::ColorPalette(const ResourceManager& resources, const IThemeProvider& theme)
    : m_resources(resources),
      m_theme(theme),
      m_blobs {{
          { 3, {} },   // Red
          { 2, {} },   // Green
          { 1, {} },   // Blue
          { 4, {} },   // White
          { 0, {} }    // Black
      }}
{
    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_clearButton);
    RenderUtils::styleAccentButton(m_undoButton, juce::Colour(0xFF4A90D9));
    RenderUtils::styleAccentButton(m_clearButton, juce::Colour(0xFFE74C3C));

    // Attach click listeners to buttons
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

void ColorPalette::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& texture = m_resources.getTexture(ResourceManager::TextureId::ColorPaletteBody);

    if (texture.isValid())
        g.drawImage(texture, bounds.getX(), bounds.getY(),
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, texture.getWidth(), texture.getHeight());

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const auto& blob = m_blobs[static_cast<size_t>(i)];
        const bool isSelected = blob.color == m_selectedColor;
        const bool isHovered  = i == m_hoveredBlob;

        auto colour = PixelCanvasComponent::colourForPixel(static_cast<PixelCanvasComponent::PixelColor>(blob.color));
        if (isHovered) colour = colour.brighter(0.12f);

        const auto drawBounds = isSelected ? blob.bounds.expanded(2.0f) : blob.bounds;

        if (isSelected)
        {
            for (int g2 = 4; g2 >= 1; --g2)
            {
                g.setColour(colour.withAlpha(0.04f));
                g.fillEllipse(drawBounds.expanded(static_cast<float>(g2) * 2.0f));
            }
        }

        juce::ColourGradient gradient(colour.brighter(0.04f),
                                      drawBounds.getCentreX(), drawBounds.getCentreY(),
                                      colour.darker(0.18f),
                                      drawBounds.getRight(), drawBounds.getBottom(),
                                      true);
        gradient.addColour(0.6, colour);
        g.setGradientFill(gradient);
        g.fillEllipse(drawBounds);

        g.setColour(colour.darker(0.30f).withAlpha(0.22f));
        g.drawEllipse(drawBounds, 1.0f);

        g.setColour(colour.brighter(0.25f).withAlpha(0.18f));
        g.drawEllipse(drawBounds.reduced(1.5f), 1.0f);
    }
}

void ColorPalette::resized()
{
    auto area = getLocalBounds()
        .withTrimmedLeft(Layout::PaletteLeftMargin)
        .withTrimmedRight(Layout::PaletteRightMargin)
        .withTrimmedTop(Layout::PaletteTopMargin)
        .withTrimmedBottom(Layout::PaletteBottomMargin);

    const auto blobSize = juce::jmin(area.getHeight() - 10.0f, Layout::BlobMaxSize);
    float startX = area.getX() + blobSize * 0.25f + 1.0f;
    float blobY = area.getCentreY() - blobSize / 2.0f - 1.0f;

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        auto slot = juce::Rectangle<float>(startX + static_cast<float>(i) * (blobSize + Layout::BlobSpacing), blobY, blobSize, blobSize);
        m_blobs[static_cast<size_t>(i)].bounds = slot;
    }

    const auto totalH = Layout::ButtonHeight * 2 + Layout::ButtonSpacing;
    const int buttonW = Layout::ButtonWidth * 2;
    const int buttonY = area.getCentreY() - totalH / 2 + 2 - 1;
    const int rightX = area.getRight() - Layout::PaletteButtonRightPadding - buttonW - 5;
    m_undoButton.setBounds(rightX, buttonY, buttonW, Layout::ButtonHeight);
    m_clearButton.setBounds(rightX, buttonY + Layout::ButtonHeight + Layout::ButtonSpacing, buttonW, Layout::ButtonHeight);
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

void ColorPalette::setSelectedColor(uint8_t color)
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