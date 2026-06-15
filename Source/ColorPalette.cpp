#include "ColorPalette.h"
#include "GridLayout.h"
#include "RenderUtils.h"
#include "ResourceManager.h"
#include "ThemeManager.h"

namespace PaletteLayout
{
    constexpr float Blob0CenterX = 0.135f;
    constexpr float BlobSpacingRatio = 0.1419f;
    constexpr float BlobSizeRatio = 0.37f;
    constexpr float BlobCenterY = 0.48f;

    constexpr float ButtonAreaCenterX = 0.867f;
    constexpr float ButtonAreaWidthRatio = 0.125f;
    constexpr float UndoButtonCenterY = 0.385f;
    constexpr float ClearButtonCenterY = 0.615f;
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

    const auto& wellTex = m_resources.getTexture(ResourceManager::TextureId::ColorWell);
    if (wellTex.isValid())
    {
        for (const auto& blob : m_blobs)
        {
            const float wellSize = blob.bounds.getWidth() * 1.5f;
            const float cx = blob.bounds.getCentreX();
            const float cy = blob.bounds.getCentreY();
            g.drawImage(wellTex,
                        cx - wellSize * 0.5f, cy - wellSize * 0.5f,
                        wellSize, wellSize,
                        0, 0, wellTex.getWidth(), wellTex.getHeight());
        }
    }

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const auto& blob = m_blobs[static_cast<size_t>(i)];
        const bool isSelected = blob.color == m_selectedColor;
        const bool isHovered  = i == m_hoveredBlob;

        auto colour = m_theme.canvasPixelColour(blob.color);
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

        const float w = drawBounds.getWidth();
        const float h = drawBounds.getHeight();
        const auto center = drawBounds.getCentre();

        // PASS 1: Cavity Depth (Well Base Shadow)
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(drawBounds);

        g.setColour(colour.darker(0.1f));
        g.fillEllipse(drawBounds.reduced(w * 0.04f));

        // PASS 2: Outer Swirl Ridge
        auto tier1 = drawBounds.reduced(w * 0.08f);
        g.setColour(colour);
        g.fillEllipse(tier1);

        {
            juce::ColourGradient shadow1(
                juce::Colours::transparentBlack, center.getX(), center.getY(),
                juce::Colours::black.withAlpha(0.35f),
                center.getX() + w * 0.4f, center.getY() + h * 0.4f, true);
            g.setGradientFill(shadow1);
            g.fillEllipse(tier1);
        }

        {
            juce::ColourGradient light1(
                juce::Colours::white.withAlpha(0.25f),
                center.getX() - w * 0.3f, center.getY() - h * 0.3f,
                juce::Colours::transparentWhite, center.getX(), center.getY(), true);
            g.setGradientFill(light1);
            g.fillEllipse(tier1);
        }

        // PASS 3: Middle Swirl Ridge (offset for spiral effect)
        auto tier2 = drawBounds.reduced(w * 0.24f).translated(-w * 0.02f, -h * 0.02f);
        g.setColour(colour.brighter(0.05f));
        g.fillEllipse(tier2);

        {
            juce::ColourGradient shadow2(
                juce::Colours::transparentBlack, tier2.getCentreX(), tier2.getCentreY(),
                juce::Colours::black.withAlpha(0.4f),
                tier2.getCentreX() + tier2.getWidth() * 0.5f,
                tier2.getCentreY() + tier2.getHeight() * 0.5f, true);
            g.setGradientFill(shadow2);
            g.fillEllipse(tier2);
        }

        {
            juce::ColourGradient light2(
                juce::Colours::white.withAlpha(0.35f),
                tier2.getCentreX() - tier2.getWidth() * 0.4f,
                tier2.getCentreY() - tier2.getHeight() * 0.4f,
                juce::Colours::transparentWhite, tier2.getCentreX(), tier2.getCentreY(), true);
            g.setGradientFill(light2);
            g.fillEllipse(tier2);
        }

        // PASS 4: Peak Tip (final twist + specular glare)
        auto peak = drawBounds.reduced(w * 0.38f).translated(-w * 0.04f, -h * 0.04f);
        g.setColour(colour.brighter(0.12f));
        g.fillEllipse(peak);

        {
            float glossSize = peak.getWidth() * 0.3f;
            auto glossCenter = juce::Point<float>(
                peak.getCentreX() - peak.getWidth() * 0.2f,
                peak.getCentreY() - peak.getHeight() * 0.2f);
            juce::ColourGradient specularPinpoint(
                juce::Colours::white.withAlpha(0.85f),
                glossCenter.getX(), glossCenter.getY(),
                juce::Colours::transparentWhite,
                glossCenter.getX() + glossSize, glossCenter.getY() + glossSize, true);
            g.setGradientFill(specularPinpoint);
            g.fillEllipse(glossCenter.getX() - glossSize,
                          glossCenter.getY() - glossSize,
                          glossSize * 2.0f, glossSize * 2.0f);
        }
    }
}

void ColorPalette::resized()
{
    auto bounds = getLocalBounds().toFloat();
    const float paletteW = bounds.getWidth();
    const float paletteH = bounds.getHeight();

    const float blobMaxFromRatio = paletteH * GridLayout::BlobMaxSizeRatio;
    const float blobSizeFromRatio = paletteH * PaletteLayout::BlobSizeRatio;
    const float blobSize = juce::jmin(blobSizeFromRatio, blobMaxFromRatio);
    const float halfBlob = blobSize * 0.5f;

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const float centerX = paletteW * (PaletteLayout::Blob0CenterX +
                      static_cast<float>(i) * PaletteLayout::BlobSpacingRatio);
        const float centerY = paletteH * PaletteLayout::BlobCenterY;
        m_blobs[static_cast<size_t>(i)].bounds = juce::Rectangle<float>(
            centerX - halfBlob, centerY - halfBlob, blobSize, blobSize);
    }

    const float buttonW = PaletteLayout::ButtonAreaWidthRatio * paletteW;
    const float buttonH = paletteH * GridLayout::ButtonHeightRatio;
    const float buttonX = paletteW * PaletteLayout::ButtonAreaCenterX - buttonW * 0.5f;

    const float undoY = paletteH * PaletteLayout::UndoButtonCenterY - buttonH * 0.5f;
    const float clearY = paletteH * PaletteLayout::ClearButtonCenterY - buttonH * 0.5f;

    m_undoButton.setBounds(static_cast<int>(buttonX), static_cast<int>(undoY),
                          static_cast<int>(buttonW), static_cast<int>(buttonH));
    m_clearButton.setBounds(static_cast<int>(buttonX), static_cast<int>(clearY),
                          static_cast<int>(buttonW), static_cast<int>(buttonH));
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
