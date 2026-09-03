#include "UI/Canvas/ArcButton.h"
#include "UI/Theme/ModernButtonLNF.h"
#include "RenderUtils.h"
#include <cmath>

ArcButton::ArcButton()
{
    setInterceptsMouseClicks(true, false);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(false);
    setBufferedToImage(false);
}

void ArcButton::setArc(float cx, float cy, float innerR, float outerR,
                        float startAngle, float endAngle)
{
    setArc(cx, cy, innerR, outerR, startAngle, endAngle, startAngle, endAngle);
}

void ArcButton::setArc(float cx, float cy, float innerR, float outerR,
                        float innerStart, float innerEnd,
                        float outerStart, float outerEnd)
{
    m_centreX = cx;
    m_centreY = cy;
    m_innerR = innerR;
    m_outerR = outerR;
    m_innerStartAngle = innerStart;
    m_innerEndAngle = innerEnd;
    m_outerStartAngle = outerStart;
    m_outerEndAngle = outerEnd;

    auto pt = [cx, cy](float r, float a) {
        return juce::Point<float>(cx + r * std::cos(a), cy + r * std::sin(a));
    };

    juce::Path path;

    const float innerSpan = innerEnd - innerStart;
    const float outerSpan = outerEnd - outerStart;
    const bool fullCircle = innerR <= 0.0f
        && std::abs(innerSpan) >= juce::MathConstants<float>::twoPi - 0.01f
        && std::abs(outerSpan) >= juce::MathConstants<float>::twoPi - 0.01f;

    if (fullCircle)
    {
        path.addEllipse(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
    }
    else
    {
        const float maxSpan = juce::jmax(std::abs(innerSpan), std::abs(outerSpan));
        const int segments = juce::jmax(4, static_cast<int>(std::ceil(maxSpan / 0.12f)));
        // Outer arc
        path.startNewSubPath(pt(outerR, outerStart));
        for (int i = 1; i <= segments; ++i)
        {
            float a = outerStart + (outerEnd - outerStart) * static_cast<float>(i) / static_cast<float>(segments);
            path.lineTo(pt(outerR, a));
        }
        // Right side — straight line to inner arc
        path.lineTo(pt(innerR, innerEnd));
        // Inner arc — reversed direction
        for (int i = segments - 1; i >= 0; --i)
        {
            float a = innerStart + (innerEnd - innerStart) * static_cast<float>(i) / static_cast<float>(segments);
            path.lineTo(pt(innerR, a));
        }
        // Close — line back to outer arc start
        path.closeSubPath();
    }

    auto bounds = path.getBounds().expanded(2.5f);
    int bx = static_cast<int>(std::floor(bounds.getX()));
    int by = static_cast<int>(std::floor(bounds.getY()));
    int bw = static_cast<int>(std::ceil(bounds.getRight())) - bx;
    int bh = static_cast<int>(std::ceil(bounds.getBottom())) - by;
    path.applyTransform(juce::AffineTransform::translation(-static_cast<float>(bx), -static_cast<float>(by)));
    m_arcPath = std::move(path);

    const float iconRadius = fullCircle ? outerR * 0.56f : (innerR + outerR) * 0.5f;
    const float iconAngle = fullCircle ? 0.0f : (innerStart + innerEnd) * 0.5f;
    const auto iconCentre = fullCircle ? juce::Point<float>(cx, cy) : pt(iconRadius, iconAngle);
    const float iconSize = fullCircle
        ? juce::jmax(0.0f, outerR * 1.15f)
        : juce::jmax(0.0f, juce::jmin(
            outerR - innerR - 4.0f,
            2.0f * iconRadius * std::sin(juce::jmin(std::abs(innerSpan), std::abs(outerSpan)) * 0.5f) - 4.0f));
    m_iconBounds = juce::Rectangle<float>(iconCentre.x - iconSize * 0.5f - static_cast<float>(bx),
                                          iconCentre.y - iconSize * 0.5f - static_cast<float>(by),
                                          iconSize, iconSize);

    setBounds(bx, by, bw, bh);
    repaint();
}

void ArcButton::setToggleState(bool on, juce::NotificationType n)
{
    juce::ignoreUnused(n);
    if (m_toggleOn == on)
        return;
    m_toggleOn = on;
    repaint();
}

void ArcButton::paint(juce::Graphics& g)
{
    auto bodyColour = juce::Colour(0xFF2A2D30);
    auto drawPath = m_arcPath;
    auto bounds = getLocalBounds().toFloat();

    if (m_toggleOn)
    {
        bodyColour = bodyColour.interpolatedWith(m_accent, 0.65f);
        const auto centre = m_arcPath.getBounds().getCentre();
        juce::AffineTransform t = juce::AffineTransform::scale(1.08f, 1.08f, centre.x, centre.y);
        drawPath.applyTransform(t);

        g.saveState();
        g.setColour(m_accent.withAlpha(0.55f));
        g.fillPath(drawPath);
        g.setColour(m_accent.withAlpha(0.85f));
        g.strokePath(drawPath, juce::PathStrokeType(1.8f));
        g.restoreState();
    }

    ModernButtonLNF::drawBody(g, drawPath, bounds,
                              bodyColour, m_hovered, m_pressed, hasKeyboardFocus(true));

    {
        g.saveState();
        g.reduceClipRegion(drawPath);
        g.setOpacity(0.03f);
        g.drawImage(RenderUtils::getNoiseTexture(),
                    getLocalBounds().toFloat(),
                    juce::RectanglePlacement::fillDestination);
        g.setOpacity(1.0f);
        g.restoreState();
    }

    if (m_drawIcon)
    {
        g.saveState();
        g.reduceClipRegion(drawPath);
        g.setColour(m_toggleOn ? m_accent.brighter(0.20f) : m_accent);
        m_drawIcon(g, m_iconBounds);
        g.restoreState();
    }
}

bool ArcButton::hitTest(int x, int y)
{
    return m_arcPath.contains(static_cast<float>(x), static_cast<float>(y));
}

void ArcButton::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isLeftButtonDown())
        return;

    if (hasKeyboardFocus(true))
        giveAwayKeyboardFocus();

    m_pressed = true;
    m_clickCancelled = false;
    repaint();
}

void ArcButton::mouseDrag(const juce::MouseEvent& e)
{
    if (!m_pressed && !m_clickCancelled)
        return;

    if (!m_arcPath.contains(e.position))
        m_clickCancelled = true;

    const bool pressed = m_pressed && !m_clickCancelled;
    if (pressed != m_pressed)
    {
        m_pressed = pressed;
        repaint();
    }
}

void ArcButton::mouseUp(const juce::MouseEvent& e)
{
    const bool activate = m_pressed && !m_clickCancelled && m_arcPath.contains(e.position);
    m_pressed = false;
    m_clickCancelled = false;
    repaint();

    if (!activate)
        return;

    if (m_toggleable)
        setToggleState(!m_toggleOn);
    if (onClick)
        onClick();
}

void ArcButton::mouseEnter(const juce::MouseEvent&)
{
    m_hovered = true;
    repaint();
}

void ArcButton::mouseExit(const juce::MouseEvent&)
{
    m_hovered = false;
    if (m_pressed)
    {
        m_pressed = false;
        m_clickCancelled = true;
    }
    repaint();
}

bool ArcButton::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() != juce::KeyPress::spaceKey
        && key.getKeyCode() != juce::KeyPress::returnKey)
        return false;

    if (m_toggleable)
        setToggleState(!m_toggleOn);
    if (onClick)
        onClick();
    return true;
}
