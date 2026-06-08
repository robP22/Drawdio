#include "ColorPalette.h"
#include "PixelCanvasComponent.h"
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
}

namespace
{
constexpr float BlobSpacing = 10.5f;
constexpr float BlobMaxSize = 72.0f;
constexpr int ButtonWidth = 38;
constexpr int ButtonHeight = 38;
constexpr int ButtonSpacing = 6;
}

ColorPalette::ColorPalette(const ResourceManager& resources, const ThemeManager& theme)
    : m_resources(resources),
      m_theme(theme),
      m_blobs {{
          { PixelCanvasComponent::PixelColor::Red,   {} },
          { PixelCanvasComponent::PixelColor::Green, {} },
          { PixelCanvasComponent::PixelColor::Blue,  {} },
          { PixelCanvasComponent::PixelColor::White, {} },
          { PixelCanvasComponent::PixelColor::Black, {} }
      }}
{
    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_clearButton);
    styleButton(m_undoButton, juce::Colour(0xFF4A90D9));
    styleButton(m_clearButton, juce::Colour(0xFFE74C3C));
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

        auto colour = PixelCanvasComponent::colourForPixel(blob.color);
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

    const auto blobSize = juce::jmin(area.getHeight() - 10.0f, BlobMaxSize);
    float startX = area.getX() + blobSize * 0.25f + 1.0f;
    float blobY = area.getCentreY() - blobSize / 2.0f - 1.0f;

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        auto slot = juce::Rectangle<float>(startX + static_cast<float>(i) * (blobSize + BlobSpacing), blobY, blobSize, blobSize);
        m_blobs[static_cast<size_t>(i)].bounds = slot;
    }

    const auto totalH = ButtonHeight * 2 + ButtonSpacing;
    const int buttonW = ButtonWidth * 2;
    const int buttonY = area.getCentreY() - totalH / 2 + 2 - 1;
    const int rightX = area.getRight() - Layout::PaletteButtonRightPadding - buttonW - 5;
    m_undoButton.setBounds(rightX, buttonY, buttonW, ButtonHeight);
    m_clearButton.setBounds(rightX, buttonY + ButtonHeight + ButtonSpacing, buttonW, ButtonHeight);
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

void ColorPalette::styleButton(juce::TextButton& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF25292C));
    button.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.2f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}