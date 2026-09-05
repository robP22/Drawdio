#include "SplitPill.h"

int SplitPill::getPreferredWidth(float pillHeight) const
{
    const float pad = pillHeight * m_padScale;
    juce::Font leftFont { juce::FontOptions(std::max(7.0f, pillHeight * m_leftFontScale)).withStyle("Bold") };
    juce::Font rightFont { juce::FontOptions(std::max(7.0f, pillHeight * m_rightFontScale)).withStyle("Bold") };
    const float leftW = juce::GlyphArrangement::getStringWidth(leftFont, m_leftText) + pad * 2.0f;
    const float rightW = juce::GlyphArrangement::getStringWidth(rightFont, m_rightText) + pad * 2.0f;
    return static_cast<int>(std::ceil(leftW + rightW + 1.0f));
}

void SplitPill::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 2.0f || bounds.getHeight() < 2.0f)
        return;

    const float h = bounds.getHeight();
    const float w = bounds.getWidth();
    const float midX = bounds.getX() + w * 0.5f;

    juce::Path body;
    body.addRoundedRectangle(bounds, h * 0.5f);

    bool anyDown = m_leftDown || m_rightDown;
    auto bodyColour = juce::Colour(0xE6151A1D);
    if (anyDown)
        bodyColour = bodyColour.darker(0.15f);
    g.setColour(bodyColour);
    g.fillPath(body);

    auto lineColour = juce::Colour(0xFF283034);
    bool leftHover = m_leftHovered && isEnabled();
    bool rightHover = m_rightHovered && isEnabled();
    bool anyHover = (leftHover || rightHover) && !anyDown;

    if (anyHover)
    {
        lineColour = juce::Colour(0xFFD3A84D);
        g.setColour(juce::Colour(0x3AD3A84D));

        juce::Path glowClip;
        if (leftHover && !rightHover)
        {
            juce::Path half;
            half.addRectangle(bounds.withWidth(w * 0.5f + 1.0f));
            glowClip = body;
            glowClip.addPath(half);
            g.saveState();
            g.reduceClipRegion(half);
            g.strokePath(body, juce::PathStrokeType(h * 0.16f));
            g.restoreState();
        }
        else if (rightHover && !leftHover)
        {
            juce::Path half;
            half.addRectangle(bounds.withTrimmedLeft(w * 0.5f - 1.0f));
            g.saveState();
            g.reduceClipRegion(half);
            g.strokePath(body, juce::PathStrokeType(h * 0.16f));
            g.restoreState();
        }
        else
        {
            g.strokePath(body, juce::PathStrokeType(h * 0.16f));
        }
    }

    g.setColour(lineColour);
    g.strokePath(body, juce::PathStrokeType(1.0f));

    g.saveState();
    g.reduceClipRegion(body);

    if (leftHover && isEnabled() && !anyDown)
    {
        g.setColour(juce::Colour(0x28D3A84D));
        g.fillRect(bounds.withWidth(w * 0.5f));
    }
    if (rightHover && isEnabled() && !anyDown)
    {
        g.setColour(juce::Colour(0x28D3A84D));
        g.fillRect(bounds.withTrimmedLeft(w * 0.5f));
    }

    g.setColour(lineColour);
    g.drawLine(midX, bounds.getY() + 1.0f, midX, bounds.getBottom() - 1.0f, 1.0f);

    g.restoreState();

    const float leftFontSize = std::max(7.0f, h * m_leftFontScale);
    const float rightFontSize = std::max(7.0f, h * m_rightFontScale);
    juce::Font leftFont { juce::FontOptions(leftFontSize).withStyle("Bold") };
    juce::Font rightFont { juce::FontOptions(rightFontSize).withStyle("Bold") };

    auto leftBounds = bounds.withWidth(w * 0.5f);
    auto rightBounds = bounds.withTrimmedLeft(w * 0.5f);

    g.setColour(juce::Colour(0xFFE8F1F5));
    g.setFont(leftFont);
    g.drawText(m_leftText, leftBounds, juce::Justification::centred, false);

    g.setColour(juce::Colour(0xFFE8F1F5));
    g.setFont(rightFont);
    g.drawText(m_rightText, rightBounds, juce::Justification::centred, false);

    if (hasKeyboardFocus(true) && isEnabled())
    {
        g.setColour(juce::Colours::gold.withAlpha(0.7f));
        g.strokePath(body, juce::PathStrokeType(1.5f));
    }
}

void SplitPill::updateHover(int x)
{
    const bool left = x < getWidth() / 2;
    const bool newLeft = left && isMouseOver(true);
    const bool newRight = !left && isMouseOver(true);
    if (newLeft != m_leftHovered || newRight != m_rightHovered)
    {
        m_leftHovered = newLeft;
        m_rightHovered = newRight;
        updateTooltip();
        repaint();
    }
}

void SplitPill::updateTooltip()
{
    if (m_leftHovered && !m_leftTip.isEmpty())
        setTooltip(m_leftTip);
    else if (m_rightHovered && !m_rightTip.isEmpty())
        setTooltip(m_rightTip);
    else
        setTooltip({});
}

void SplitPill::mouseEnter(const juce::MouseEvent& e)
{
    updateHover(e.x);
}

void SplitPill::mouseMove(const juce::MouseEvent& e)
{
    updateHover(e.x);
}

void SplitPill::mouseExit(const juce::MouseEvent&)
{
    if (m_leftHovered || m_rightHovered)
    {
        m_leftHovered = false;
        m_rightHovered = false;
        updateTooltip();
        repaint();
    }
}

void SplitPill::mouseDown(const juce::MouseEvent& e)
{
    if (hasKeyboardFocus(true))
        giveAwayKeyboardFocus();
    if (!isEnabled())
        return;
    const bool left = isLeftHalf(e.x);
    m_activeSide = left ? 0 : 1;
    if (left)
        m_leftDown = true;
    else
        m_rightDown = true;
    repaint();
}

void SplitPill::mouseUp(const juce::MouseEvent& e)
{
    const bool wasLeftDown = m_leftDown;
    const bool wasRightDown = m_rightDown;
    m_leftDown = false;
    m_rightDown = false;

    const bool inside = contains(e.getPosition());
    const bool left = isLeftHalf(e.x);
    updateHover(e.x);

    if (inside)
    {
        if (left && wasLeftDown && onLeftClick)
            onLeftClick();
        else if (!left && wasRightDown && onRightClick)
            onRightClick();
    }
    juce::ignoreUnused(wasRightDown);
    repaint();
}

bool SplitPill::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
    {
        if (m_leftHovered && onLeftClick) { onLeftClick(); return true; }
        if (m_rightHovered && onRightClick) { onRightClick(); return true; }
        if (onLeftClick) { onLeftClick(); return true; }
    }
    if (key == juce::KeyPress::leftKey)
    {
        m_leftHovered = true;
        m_rightHovered = false;
        updateTooltip();
        repaint();
        return true;
    }
    if (key == juce::KeyPress::rightKey)
    {
        m_leftHovered = false;
        m_rightHovered = true;
        updateTooltip();
        repaint();
        return true;
    }
    return false;
}
