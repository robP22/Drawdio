#include "ColorPalette.h"
#include "PixelCanvasComponent.h"
#include "RenderUtils.h"
#include "ResourceManager.h"
#include "ThemeManager.h"

namespace PaletteLayout
{
    // Blob layout (5 blobs): centers as ratio of palette width
    constexpr float Blob0CenterX = 0.15f;  // First blob at 15% of width
    constexpr float BlobSpacingRatio = 0.15f;  // Each blob spaced by 15% of width
    constexpr float BlobSizeRatio = 0.45f;  // Blob diameter = 45% of palette height
    constexpr float BlobMaxSize = 72.0f;  // Maximum blob size in pixels
    constexpr float BlobCenterY = 0.50f;  // Vertically centered
    
    // Button layout: positioned at right side
    constexpr float ButtonAreaCenterX = 0.85f;  // Buttons at 85% of width
    constexpr float ButtonAreaWidthRatio = 0.25f;  // Button area = 25% of palette width
    constexpr float UndoButtonCenterY = 0.38f;  // Undo at 38% of palette height
    constexpr float ClearButtonCenterY = 0.62f;  // Clear at 62% of palette height
    constexpr int ButtonWidth = 38;
    constexpr int ButtonHeight = 38;
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
    auto bounds = getLocalBounds().toFloat();
    const float paletteW = bounds.getWidth();
    const float paletteH = bounds.getHeight();
    
    // Calculate blob size (bounded by ratio and max pixel size)
    const float blobSizeFromRatio = paletteH * PaletteLayout::BlobSizeRatio;
    const float blobSize = juce::jmin(blobSizeFromRatio, PaletteLayout::BlobMaxSize);
    const float halfBlob = blobSize * 0.5f;
    
    // Position blobs using normalized center ratios
    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const float centerX = paletteW * (PaletteLayout::Blob0CenterX + 
                      static_cast<float>(i) * PaletteLayout::BlobSpacingRatio);
        const float centerY = paletteH * PaletteLayout::BlobCenterY;
        m_blobs[static_cast<size_t>(i)].bounds = juce::Rectangle<float>(
            centerX - halfBlob, centerY - halfBlob, blobSize, blobSize);
    }
    
    // Position buttons using normalized ratios
    const float buttonW = PaletteLayout::ButtonAreaWidthRatio * paletteW;
    const float buttonX = paletteW * PaletteLayout::ButtonAreaCenterX - buttonW * 0.5f;
    
    const float undoY = paletteH * PaletteLayout::UndoButtonCenterY - PaletteLayout::ButtonHeight * 0.5f;
    const float clearY = paletteH * PaletteLayout::ClearButtonCenterY - PaletteLayout::ButtonHeight * 0.5f;
    
    m_undoButton.setBounds(static_cast<int>(buttonX), static_cast<int>(undoY), 
                          static_cast<int>(buttonW), PaletteLayout::ButtonHeight);
    m_clearButton.setBounds(static_cast<int>(buttonX), static_cast<int>(clearY), 
                          static_cast<int>(buttonW), PaletteLayout::ButtonHeight);
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
