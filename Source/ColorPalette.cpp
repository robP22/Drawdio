#include "ColorPalette.h"
#include "GridLayout.h"
#include "RenderUtils.h"
#include "ResourceManager.h"
#include "ThemeManager.h"

namespace PaletteLayout
{
    constexpr float Blob0CenterX = 0.135f;
    constexpr float BlobSpacingRatio = 0.057f;
    constexpr float BlobSizeRatio = 0.242f;
    constexpr float BlobCenterY0 = 0.36f;
    constexpr float BlobCenterY1 = 0.64f;
}

ColorPalette::ColorPalette(const ResourceManager& resources, const IThemeProvider& theme)
    : m_resources(resources),
      m_theme(theme),
      m_blobs {{
          { 0, {} },   // Black   (weight -1.0)
          { 7, {} },   // Brown   (weight -0.8)
          { 8, {} },   // Purple  (weight -0.6)
          { 1, {} },   // Blue    (weight -0.4)
          { 2, {} },   // Green   (weight -0.2)
          { 9, {} },   // Grey    (weight  0.0)
          { 10, {} },  // Pink    (weight +0.2)
          { 6, {} },   // Yellow  (weight +0.4)
          { 3, {} },   // Red     (weight +0.6)
          { 4, {} }    // White   (weight +1.0)
      }}
{
    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_clearButton);
    addAndMakeVisible(m_fillButton);
    addAndMakeVisible(m_sizeButton);
    RenderUtils::styleAccentButton(m_undoButton, juce::Colour(0xFF4A90D9));
    RenderUtils::styleAccentButton(m_clearButton, juce::Colour(0xFFE74C3C));
    RenderUtils::styleAccentButton(m_fillButton, juce::Colour(0xFF2ECC40));
    RenderUtils::styleAccentButton(m_sizeButton, juce::Colour(0xFFF39C12));

    m_fillButton.setClickingTogglesState(true);
    m_sizeButton.setButtonText("Fine");

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

    m_fillButton.onClick = [this]()
    {
        if (m_onFill)
            m_onFill(m_fillButton.getToggleState());
    };

    m_sizeButton.onClick = [this]()
    {
        cycleBrushSize();
    };
}

void ColorPalette::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& texture = m_resources.getTexture(ResourceManager::TextureId::ColorPaletteBody);

    if (texture.isValid())
    {
        const float offsetX = m_imageCenterX - bounds.getWidth() * 0.5f;
        g.drawImage(texture,
                   bounds.getX() + offsetX, bounds.getY() - m_imageVerticalShift,
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, texture.getWidth(), texture.getHeight());
    }

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
        const float centerY = paletteH * ((i % 2 == 0) ? PaletteLayout::BlobCenterY0
                                                        : PaletteLayout::BlobCenterY1)
                              - m_imageVerticalShift
                              + m_contentCenterOffset;
        m_blobs[static_cast<size_t>(i)].bounds = juce::Rectangle<float>(
            centerX - halfBlob, centerY - halfBlob, blobSize, blobSize);
    }

    const float buttonSize = paletteH * GridLayout::ButtonSquareSizeRatio;
    const float gap = 5.0f;
    const float gridW = 2.0f * buttonSize + gap;
    const float gridH = 2.0f * buttonSize + gap;

    const float groupTop = (paletteH - gridH) * 0.5f - m_imageVerticalShift + m_contentCenterOffset;
    const float groupLeft = paletteW - groupTop - gridW;

    auto setBtn = [&](juce::TextButton& btn, float x, float y) {
        btn.setBounds(static_cast<int>(x), static_cast<int>(y),
                      static_cast<int>(buttonSize), static_cast<int>(buttonSize));
    };

    setBtn(m_undoButton, groupLeft,               groupTop);
    setBtn(m_clearButton, groupLeft + buttonSize + gap, groupTop);
    setBtn(m_fillButton,  groupLeft,               groupTop + buttonSize + gap);
    setBtn(m_sizeButton,  groupLeft + buttonSize + gap, groupTop + buttonSize + gap);
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

void ColorPalette::cycleBrushSize()
{
    m_currentBrushIndex = (m_currentBrushIndex + 1) % 3;
    float radius = m_brushSizes[m_currentBrushIndex];
    static const char* labels[] = {"Fine", "Med", "Broad"};
    m_sizeButton.setButtonText(labels[m_currentBrushIndex]);
    if (m_onBrushSize)
        m_onBrushSize(radius);
}
