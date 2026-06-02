#include "PedalComponent.h"
#include "PluginProcessor.h"

#include <cmath>

// ============================================================================
// 8-Lobe Scalloped Knob LookAndFeel - Method Implementation
// ============================================================================

void PedalComponent::PedalKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                                           juce::Slider& slider)
{
    const float diameter = static_cast<float>(juce::jmin(w, h));
    const float radius = diameter * 0.5f;
    const float centerX = static_cast<float>(x) + radius;
    const float centerY = static_cast<float>(y) + radius;

    const float value = static_cast<float>(slider.getValue());
    const float normalizedValue = (value - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum());

    // 270-degree arc: from ~7 o'clock to ~5 o'clock
    const float startAngle = rotaryStartAngle;
    const float endAngle = rotaryEndAngle;
    const float angleRange = endAngle - startAngle;
    const float currentAngle = startAngle + normalizedValue * angleRange;

    // === KNOB SHADOW (static, offset slightly down-right) ===
    const float shadowOffsetX = 2.0f;
    const float shadowOffsetY = 3.0f;

    juce::Path shadowPath;
    shadowPath.addEllipse(centerX - radius + shadowOffsetX,
                          centerY - radius + shadowOffsetY,
                          diameter, diameter);
    g.setColour(juce::Colour(0x40000000));
    g.fillPath(shadowPath);

    // === KNOB BODY (8-lobe scalloped) ===
    const int numLobes = 8;
    const float lobeDepth = radius * 0.08f;  // Scallop depth

    juce::Path bodyPath;
    for (int i = 0; i <= numLobes * 2; ++i)
    {
        const float angle = (static_cast<float>(i) / static_cast<float>(numLobes * 2)) * 2.0f * juce::MathConstants<float>::pi;
        const float lobeRadius = radius - (i % 2 == 0 ? 0.0f : lobeDepth);
        const float px = centerX + std::cos(angle) * lobeRadius;
        const float py = centerY + std::sin(angle) * lobeRadius;

        if (i == 0)
            bodyPath.startNewSubPath(px, py);
        else
            bodyPath.lineTo(px, py);
    }
    bodyPath.closeSubPath();

    // Body gradient (matte black plastic)
    juce::ColourGradient bodyGradient;
    bodyGradient.addColour(0.0, juce::Colour(0xFF2A2A2A));
    bodyGradient.addColour(0.5, juce::Colour(0xFF1A1A1A));
    bodyGradient.addColour(1.0, juce::Colour(0xFF0A0A0A));
    bodyGradient.point1 = juce::Point<float>(centerX - radius, centerY - radius);
    bodyGradient.point2 = juce::Point<float>(centerX + radius, centerY + radius);

    g.setGradientFill(bodyGradient);
    g.fillPath(bodyPath);

    // === INNER SHADOW RING (recessed edge around cap) ===
    const float capRadius = radius * 0.78f;
    const float innerShadowWidth = radius * 0.08f;

    juce::Path innerShadowPath;
    innerShadowPath.addEllipse(centerX - radius, centerY - radius, diameter, diameter);

    g.setColour(juce::Colour(0xFF050505));
    g.fillPath(innerShadowPath, juce::PathStrokeType(innerShadowWidth, juce::PathStrokeType::curved));

    // === BRUSHED ALUMINUM CAP ===
    const float capDiameter = capRadius * 2.0f;

    // Cap base gradient
    juce::ColourGradient capGradient;
    capGradient.addColour(0.0, juce::Colour(0xFFD0D0D0));
    capGradient.addColour(0.3, juce::Colour(0xFFB8B8B8));
    capGradient.addColour(0.5, juce::Colour(0xFFC8C8C8));
    capGradient.addColour(1.0, juce::Colour(0xFF909090));
    capGradient.point1 = juce::Point<float>(centerX - capRadius, centerY - capRadius);
    capGradient.point2 = juce::Point<float>(centerX + capRadius, centerY + capRadius);

    g.setGradientFill(capGradient);
    g.fillEllipse(centerX - capRadius, centerY - capRadius, capDiameter, capDiameter);

    // === CONCENTRIC LATHE RINGS (6 rings) ===
    const int numRings = 6;
    const float ringStartRadius = capRadius * 0.2f;
    const float ringEndRadius = capRadius * 0.95f;
    const float ringGap = (ringEndRadius - ringStartRadius) / static_cast<float>(numRings);

    for (int i = 0; i < numRings; ++i)
    {
        const float ringRadius = ringStartRadius + i * ringGap;
        const float brightness = 0.85f + 0.15f * std::sin(static_cast<float>(i) * 0.8f);
        g.setColour(juce::Colour(static_cast<uint8_t>(brightness * 255.0f),
                                 static_cast<uint8_t>(brightness * 255.0f),
                                 static_cast<uint8_t>(brightness * 255.0f)));
        g.drawEllipse(centerX - ringRadius, centerY - ringRadius,
                      ringRadius * 2.0f, ringRadius * 2.0f, 0.5f);
    }

    // === ROTATING INDICATOR LINE (at 12 o'clock, relative to cap angle) ===
    const float indicatorWidth = 2.0f;
    const float indicatorOffset = capRadius * 0.15f;

    const float indicatorAngle = currentAngle - juce::MathConstants<float>::pi / 2.0f;

    juce::Path indicatorPath;

    const float startX = centerX + std::cos(indicatorAngle) * indicatorOffset;
    const float startY = centerY + std::sin(indicatorAngle) * indicatorOffset;

    const float endX = centerX + std::cos(indicatorAngle) * (capRadius * 0.85f);
    const float endY = centerY + std::sin(indicatorAngle) * (capRadius * 0.85f);

    const float perpX = -std::sin(indicatorAngle);
    const float perpY = std::cos(indicatorAngle);

    indicatorPath.startNewSubPath(startX - perpX * indicatorWidth, startY - perpY * indicatorWidth);
    indicatorPath.lineTo(startX + perpX * indicatorWidth, startY + perpY * indicatorWidth);
    indicatorPath.lineTo(endX + perpX * indicatorWidth, endY + perpY * indicatorWidth);
    indicatorPath.lineTo(endX - perpX * indicatorWidth, endY - perpY * indicatorWidth);
    indicatorPath.closeSubPath();

    g.setColour(juce::Colour(0xFFFFFFFF));
    g.fillPath(indicatorPath);

    // === SPECULAR HIGHLIGHT (top-left quadrant) ===
    juce::Path highlightPath;
    highlightPath.addEllipse(centerX - capRadius * 0.5f,
                             centerY - capRadius - capRadius * 0.2f,
                             capRadius * 0.6f,
                             capRadius * 0.5f);

    juce::ColourGradient highlightGrad;
    highlightGrad.addColour(0.0, juce::Colour(0x40FFFFFF));
    highlightGrad.addColour(1.0, juce::Colour(0x00FFFFFF));
    highlightGrad.point1 = juce::Point<float>(centerX - capRadius, centerY - capRadius);
    highlightGrad.point2 = juce::Point<float>(centerX, centerY);

    g.setGradientFill(highlightGrad);
    g.fillPath(highlightPath);

    // === BODY EDGE HIGHLIGHT ===
    juce::Path bodyEdgePath;
    bodyEdgePath.addEllipse(centerX - radius, centerY - radius, diameter, diameter);
    g.setColour(juce::Colour(0x30FFFFFF));
    g.strokePath(bodyEdgePath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved));
}

PedalComponent::PedalComponent(DrawdioProcessor& processor, int slotIndex, int spriteFrameX, int spriteFrameY)
    : audioProcessor(processor),
      m_slotIndex(slotIndex),
      m_spriteFrameX(spriteFrameX),
      m_spriteFrameY(spriteFrameY)
{
    loadSpriteSheet();
    initKnobs();
}

void PedalComponent::loadSpriteSheet()
{
    juce::File texturePath;

    auto assetDir = juce::File::getSpecialLocation(juce::File::invokedExecutableFile).getParentDirectory();
    texturePath = assetDir.getChildFile("Contents/Resources/Assets/Textures/pedal_sprite_sheet.png");

    if (!texturePath.existsAsFile())
    {
        texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Textures/pedal_sprite_sheet.png");
    }

    if (texturePath.existsAsFile())
    {
        m_pedalImage = juce::ImageCache::getFromFile(texturePath);
    }
}

void PedalComponent::initKnobs()
{
    for (int i = 0; i < 4; ++i)
    {
        m_knobs[i].setSliderStyle(juce::Slider::Rotary);
        m_knobs[i].setLookAndFeel(&PedalKnobLookAndFeel::getInstance());
        m_knobs[i].setRange(0.0, 1.0, 0.0);
        m_knobs[i].setValue(m_knobValues[i], juce::sendNotificationSync);
        m_knobs[i].setVisible(true);
        addAndMakeVisible(m_knobs[i]);

        m_knobs[i].onValueChange = [this, i]
        {
            m_knobValues[i] = static_cast<float>(m_knobs[i].getValue());
            repaint();
        };
    }
}

void PedalComponent::paint(juce::Graphics& g)
{
    if (!m_pedalImage.isValid())
    {
        g.fillAll(juce::Colour(0xFF1A1A1A));
        return;
    }

    auto bounds = getLocalBounds().toFloat();

    // Extract the correct region from sprite sheet
    const int srcX = m_spriteFrameX * spriteWidth();
    const int srcY = m_spriteFrameY * spriteHeight();
    const int srcW = spriteWidth();
    const int srcH = spriteHeight();

    // Draw sprite region stretched to fill
    g.drawImage(m_pedalImage,
                bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                srcX, srcY, srcW, srcH);
}

void PedalComponent::resized()
{
    auto bounds = getLocalBounds();

    if (bounds.isEmpty() || !m_pedalImage.isValid())
        return;

    // Position 4 knobs in 2x2 grid
    // These positions should match the sprite sheet's knob locations
    const int knobW = bounds.getWidth() / 5;  // Knob takes 1/5 of cell width
    const int knobH = bounds.getHeight() / 6; // Knob takes 1/6 of cell height

    // Calculate cell positions (2x2 grid)
    // Top-left cell
    auto cell0X = bounds.getX() + bounds.getWidth() / 4 - knobW / 2;
    auto cell0Y = bounds.getY() + bounds.getHeight() / 4 - knobH / 2;

    // Top-right cell
    auto cell1X = bounds.getX() + bounds.getWidth() * 3 / 4 - knobW / 2;
    auto cell1Y = bounds.getY() + bounds.getHeight() / 4 - knobH / 2;

    // Bottom-left cell
    auto cell2X = bounds.getX() + bounds.getWidth() / 4 - knobW / 2;
    auto cell2Y = bounds.getY() + bounds.getHeight() * 3 / 4 - knobH / 2;

    // Bottom-right cell
    auto cell3X = bounds.getX() + bounds.getWidth() * 3 / 4 - knobW / 2;
    auto cell3Y = bounds.getY() + bounds.getHeight() * 3 / 4 - knobH / 2;

    m_knobs[0].setBounds(static_cast<int>(cell0X), static_cast<int>(cell0Y), knobW, knobH);
    m_knobs[1].setBounds(static_cast<int>(cell1X), static_cast<int>(cell1Y), knobW, knobH);
    m_knobs[2].setBounds(static_cast<int>(cell2X), static_cast<int>(cell2Y), knobW, knobH);
    m_knobs[3].setBounds(static_cast<int>(cell3X), static_cast<int>(cell3Y), knobW, knobH);
}

void PedalComponent::setKnobValue(int knobIdx, float value)
{
    if (knobIdx < 0 || knobIdx >= 4)
        return;

    m_knobValues[knobIdx] = juce::jlimit(0.0f, 1.0f, value);
    m_knobs[knobIdx].setValue(m_knobValues[knobIdx], juce::sendNotificationSync);
}

float PedalComponent::getKnobValue(int knobIdx) const
{
    if (knobIdx < 0 || knobIdx >= 4)
        return 0.5f;

    return m_knobValues[knobIdx];
}