#include "ColorPalette.h"
#include "GridLayout.h"


namespace {

static void drawTrash(juce::Graphics& g, juce::Rectangle<float> r)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float s = std::min(r.getWidth(), r.getHeight()) * 0.5f;

    juce::Path p;
    p.addRectangle(cx - s * 0.28f, cy - s * 0.10f, s * 0.56f, s * 0.62f);
    p.addRectangle(cx - s * 0.38f, cy - s * 0.28f, s * 0.76f, s * 0.16f);
    p.addRectangle(cx - s * 0.10f, cy - s * 0.48f, s * 0.20f, s * 0.22f);
    g.fillPath(p);
}

static void drawUndoArrow(juce::Graphics& g, juce::Rectangle<float> r)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float s = std::min(r.getWidth(), r.getHeight()) * 0.5f;
    float arcR = s * 0.38f;

    juce::Path arc;
    arc.addCentredArc(cx, cy, arcR, arcR, 0.0f,
                      juce::degreesToRadians(210.0f), juce::degreesToRadians(390.0f), true);
    g.strokePath(arc, juce::PathStrokeType(s * 0.30f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float hx = cx + arcR * std::cos(juce::degreesToRadians(210.0f));
    float hy = cy + arcR * std::sin(juce::degreesToRadians(210.0f));
    juce::Path tip;
    tip.addTriangle(hx, hy,
                    hx + s * 0.26f, hy - s * 0.20f,
                    hx + s * 0.26f, hy + s * 0.20f);
    g.fillPath(tip);
}

static void drawRedoArrow(juce::Graphics& g, juce::Rectangle<float> r)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float s = std::min(r.getWidth(), r.getHeight()) * 0.5f;
    float arcR = s * 0.38f;

    juce::Path arc;
    arc.addCentredArc(cx, cy, arcR, arcR, 0.0f,
                      juce::degreesToRadians(150.0f), juce::degreesToRadians(330.0f), true);
    g.strokePath(arc, juce::PathStrokeType(s * 0.30f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float hx = cx + arcR * std::cos(juce::degreesToRadians(330.0f));
    float hy = cy + arcR * std::sin(juce::degreesToRadians(330.0f));
    juce::Path tip;
    tip.addTriangle(hx, hy,
                    hx - s * 0.26f, hy - s * 0.20f,
                    hx - s * 0.26f, hy + s * 0.20f);
    g.fillPath(tip);
}

static void drawBrushCircle(juce::Graphics& g, juce::Rectangle<float> r, int sizeIndex)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float s = std::min(r.getWidth(), r.getHeight()) * 0.5f;

    juce::Path ring;
    ring.addEllipse(cx - s * 0.36f, cy - s * 0.36f, s * 0.72f, s * 0.72f);
    g.strokePath(ring, juce::PathStrokeType(s * 0.10f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float dotR = s * (0.12f + 0.04f * static_cast<float>(sizeIndex));
    g.fillEllipse(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
}

static void drawEraser(juce::Graphics& g, juce::Rectangle<float> r)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float s = std::min(r.getWidth(), r.getHeight()) * 0.5f;

    juce::Path p;
    p.startNewSubPath(cx - s * 0.34f, cy - s * 0.20f);
    p.lineTo(cx - s * 0.10f, cy - s * 0.40f);
    p.lineTo(cx + s * 0.34f, cy - s * 0.40f);
    p.lineTo(cx + s * 0.34f, cy + s * 0.26f);
    p.lineTo(cx + s * 0.10f, cy + s * 0.40f);
    p.lineTo(cx - s * 0.34f, cy + s * 0.26f);
    p.closeSubPath();
    g.fillPath(p);

    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawLine(cx - s * 0.28f, cy + s * 0.32f,
               cx + s * 0.28f, cy + s * 0.32f,
               s * 0.08f);
}

static void drawFillBucket(juce::Graphics& g, juce::Rectangle<float> r)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float s = std::min(r.getWidth(), r.getHeight()) * 0.5f;

    juce::Path p;
    p.startNewSubPath(cx - s * 0.26f, cy - s * 0.05f);
    p.lineTo(cx + s * 0.26f, cy - s * 0.05f);
    p.lineTo(cx + s * 0.40f, cy + s * 0.42f);
    p.lineTo(cx - s * 0.40f, cy + s * 0.42f);
    p.closeSubPath();
    g.fillPath(p);

    juce::Path handle;
    handle.addArc(cx - s * 0.16f, cy - s * 0.44f, s * 0.32f, s * 0.36f,
                  juce::degreesToRadians(200.0f), juce::degreesToRadians(340.0f), true);
    g.strokePath(handle, juce::PathStrokeType(s * 0.13f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void drawReboundArrow(juce::Graphics& g, juce::Rectangle<float> r)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float s = std::min(r.getWidth(), r.getHeight()) * 0.5f;

    g.drawLine(cx + s * 0.34f, cy - s * 0.38f,
               cx + s * 0.34f, cy + s * 0.38f,
               s * 0.12f);

    juce::Path path;
    path.startNewSubPath(cx - s * 0.32f, cy - s * 0.32f);
    path.quadraticTo(cx, cy - s * 0.34f,
                     cx + s * 0.32f, cy);
    path.quadraticTo(cx, cy + s * 0.34f,
                     cx - s * 0.32f, cy + s * 0.36f);
    g.strokePath(path, juce::PathStrokeType(s * 0.14f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float tipX = cx - s * 0.32f - s * 0.06f;
    float tipY = cy + s * 0.36f;
    juce::Path head;
    head.addTriangle(tipX, tipY,
                     tipX + s * 0.22f, tipY - s * 0.14f,
                     tipX + s * 0.22f, tipY + s * 0.14f);
    g.fillPath(head);
}

}

ColorPalette::ColorPalette(const IResourceProvider& resources, const IThemeProvider& theme)
    : m_resources(resources),
      m_theme(theme),
      m_blobs {{
          { 0, {} },   // Black   (weight -1.0)
          { 7, {} },   // Brown   (weight +0.9)
          { 8, {} },   // Purple  (weight -0.55)
          { 12, {} },  // Violet  (weight +0.6)
          { 1, {} },   // Blue    (weight -0.8)
          { 2, {} },   // Green   (weight +0.55)
          { 9, {} },   // Grey    (weight  0.0)
          { 10, {} },  // Pink    (weight -0.6)
          { 6, {} },   // Yellow  (weight +0.7)
          { 11, {} },  // Orange  (weight -0.9)
          { 3, {} },   // Red     (weight +0.8)
          { 4, {} }    // White   (weight -0.7)
      }}
{
    setBufferedToImage(true);
    addAndMakeVisible(m_reboundButton);
    addAndMakeVisible(m_undoButton);
    addAndMakeVisible(m_redoButton);
    addAndMakeVisible(m_fillButton);
    addAndMakeVisible(m_sizeButton);
    addAndMakeVisible(m_eraserButton);
    addAndMakeVisible(m_clearButton);

    m_reboundButton.setDrawIcon(drawReboundArrow);
    m_reboundButton.setAccentColour(juce::Colour(0xFF8E44AD));
    m_reboundButton.setClickingTogglesState(true);
    m_reboundButton.onClick = [this]() {
        if (!m_reboundButton.getToggleState())
        {
            m_reboundButton.setAccentColour(juce::Colour(0xFF8E44AD));
            if (m_onReboundMode) m_onReboundMode(false);
            return;
        }
        setExclusiveToggle(m_reboundButton);
        m_reboundButton.setAccentColour(juce::Colour(0xFFC06DE0));
        if (m_onReboundMode)
            m_onReboundMode(true);
    };

    m_undoButton.setDrawIcon(drawUndoArrow);
    m_undoButton.setAccentColour(juce::Colour(0xFF4A90D9));
    m_undoButton.onClick = [this]() {
        if (m_onUndo) m_onUndo();
    };

    m_redoButton.setDrawIcon(drawRedoArrow);
    m_redoButton.setAccentColour(juce::Colour(0xFF4A90D9));
    m_redoButton.onClick = [this]() {
        if (m_onRedo) m_onRedo();
    };

    m_fillButton.setDrawIcon(drawFillBucket);
    m_fillButton.setAccentColour(juce::Colour(0xFF2ECC40));
    m_fillButton.setClickingTogglesState(true);
    m_fillButton.onClick = [this]() {
        if (!m_fillButton.getToggleState())
        {
            if (m_onFill) m_onFill(false);
            return;
        }
        setExclusiveToggle(m_fillButton);
        if (m_onFill)
            m_onFill(true);
    };

    m_sizeButton.setDrawIcon([this](juce::Graphics& g, juce::Rectangle<float> r) {
        drawBrushCircle(g, r, m_currentBrushIndex);
    });
    m_sizeButton.setAccentColour(juce::Colour(0xFFF39C12));
    m_sizeButton.onClick = [this]() { cycleBrushSize(); };

    m_eraserButton.setDrawIcon(drawEraser);
    m_eraserButton.setAccentColour(juce::Colour(0xFF888888));
    m_eraserButton.setClickingTogglesState(true);
    m_eraserButton.onClick = [this]() {
        bool on = m_eraserButton.getToggleState();
        if (on)
        {
            setExclusiveToggle(m_eraserButton);
            m_eraserSavedColor = m_selectedColor;
            if (m_onEraser) m_onEraser(true);
        }
        else
        {
            m_selectedColor = m_eraserSavedColor;
            if (m_onColorSelected) m_onColorSelected(m_selectedColor);
            if (m_onEraser) m_onEraser(false);
        }
    };

    m_clearButton.setDrawIcon(drawTrash);
    m_clearButton.setAccentColour(juce::Colour(0xFFE74C3C));
    m_clearButton.onClick = [this]() {
        if (m_onClear) m_onClear();
    };
}

void ColorPalette::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& texture = m_resources.getTexture(IResourceProvider::TextureId::ColorPaletteBody);

    if (texture.isValid())
    {
        const float offsetX = m_imageCenterX - bounds.getWidth() * 0.5f;
        g.drawImage(texture,
                   bounds.getX() + offsetX, bounds.getY() - m_imageVerticalShift,
                   bounds.getWidth(), bounds.getHeight(),
                   0, 0, texture.getWidth(), texture.getHeight());
    }

    const auto& wellTex = m_resources.getTexture(IResourceProvider::TextureId::ColorWell);
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

        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(drawBounds);
        g.setColour(colour.darker(0.1f));
        g.fillEllipse(drawBounds.reduced(w * 0.04f));

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
    const float blobSizeFromRatio = paletteH * GridLayout::PaletteBlobSizeRatio;
    const float blobSize = juce::jmin(blobSizeFromRatio, blobMaxFromRatio);
    const float halfBlob = blobSize * 0.5f;

    for (int i = 0; i < static_cast<int>(m_blobs.size()); ++i)
    {
        const float centerX = paletteW * (GridLayout::PaletteBlob0CenterX +
                      static_cast<float>(i) * GridLayout::PaletteBlobSpacingRatio);
        const float centerY = paletteH * ((i % 2 == 0) ? GridLayout::PaletteBlobCenterY0
                                                        : GridLayout::PaletteBlobCenterY1)
                              - m_imageVerticalShift
                              + m_contentCenterOffset;
        m_blobs[static_cast<size_t>(i)].bounds = juce::Rectangle<float>(
            centerX - halfBlob, centerY - halfBlob, blobSize, blobSize);
    }

    // Center the wheel between the rightmost blob and the right palette edge
    float lastBlobRight = paletteW * (GridLayout::PaletteBlob0CenterX +
        11.0f * GridLayout::PaletteBlobSpacingRatio) + halfBlob;
    float cx = lastBlobRight + (paletteW - lastBlobRight) * 0.5f;
    float cy = paletteH * 0.5f - m_imageVerticalShift + m_contentCenterOffset;
    float centreR = paletteH * 0.09f;
    float wheelGap = 5.0f;
    float ringInnerR = centreR + wheelGap;
    float ringOuterR = paletteH * 0.23f;

    m_clearButton.setArc(cx, cy, 0, centreR, 0, juce::MathConstants<float>::twoPi);

    float gapPx = 4.0f;
    float innerGapDeg = 360.0f * gapPx / (2.0f * juce::MathConstants<float>::pi * ringInnerR);
    float outerGapDeg = 360.0f * gapPx / (2.0f * juce::MathConstants<float>::pi * ringOuterR);

    auto setRing = [&](ArcButton& btn, int idx) {
        float midDeg = -90.0f + static_cast<float>(idx) * 60.0f;
        float innerStart = juce::degreesToRadians(midDeg + innerGapDeg * 0.5f);
        float innerEnd   = juce::degreesToRadians(midDeg + 60.0f - innerGapDeg * 0.5f);
        float outerStart = juce::degreesToRadians(midDeg + outerGapDeg * 0.5f);
        float outerEnd   = juce::degreesToRadians(midDeg + 60.0f - outerGapDeg * 0.5f);
        btn.setArc(cx, cy, ringInnerR, ringOuterR, innerStart, innerEnd, outerStart, outerEnd);
    };

    setRing(m_reboundButton, 0);
    setRing(m_redoButton,   1);
    setRing(m_sizeButton,   2);
    setRing(m_eraserButton, 3);
    setRing(m_fillButton,   4);
    setRing(m_undoButton,   5);
}

void ColorPalette::setExclusiveToggle(ArcButton& active)
{
    auto untoggle = [&active](ArcButton& other, std::function<void(bool)> offCb)
    {
        if (&other != &active && other.getToggleState())
        {
            other.setToggleState(false, juce::dontSendNotification);
            other.repaint();
            if (offCb)
                offCb(false);
        }
    };
    if (&active != &m_eraserButton && m_eraserButton.getToggleState())
    {
        m_selectedColor = m_eraserSavedColor;
        if (m_onColorSelected) m_onColorSelected(m_selectedColor);
    }
    untoggle(m_fillButton, m_onFill);
    untoggle(m_eraserButton, m_onEraser);
    untoggle(m_reboundButton, m_onReboundMode);
}

void ColorPalette::mouseDown(const juce::MouseEvent& event)
{
    const int index = hitTestBlob(event.position);
    if (index < 0)
        return;

    if (m_eraserButton.getToggleState())
    {
        m_eraserButton.setToggleState(false, juce::dontSendNotification);
        if (m_onEraser) m_onEraser(false);
    }

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
    m_currentBrushIndex = (m_currentBrushIndex + 1) % 4;
    float radius = m_brushSizes[m_currentBrushIndex];
    m_sizeButton.repaint();
    if (m_onBrushSize)
        m_onBrushSize(radius);
}
