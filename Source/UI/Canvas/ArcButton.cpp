#include "UI/Canvas/ArcButton.h"
#include "RenderUtils.h"
#include <cmath>

ArcButton::ArcButton()
{
    setInterceptsMouseClicks(true, false);
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
    m_startAngle = innerStart;
    m_endAngle = innerEnd;

    auto pt = [cx, cy](float r, float a) {
        return juce::Point<float>(cx + r * std::cos(a), cy + r * std::sin(a));
    };

    juce::Path path;

    if (innerR == 0.0f && (innerEnd - innerStart) >= juce::MathConstants<float>::twoPi - 0.01f)
    {
        path.addEllipse(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);
    }
    else
    {
        static constexpr int segments = 12;
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

    auto bounds = path.getBounds();
    int bx = static_cast<int>(std::floor(bounds.getX()));
    int by = static_cast<int>(std::floor(bounds.getY()));
    int bw = static_cast<int>(std::ceil(bounds.getRight())) - bx;
    int bh = static_cast<int>(std::ceil(bounds.getBottom())) - by;
    setBounds(bx, by, bw, bh);

    m_arcPath = std::move(path);
    m_arcPath.applyTransform(juce::AffineTransform::translation(-static_cast<float>(bx), -static_cast<float>(by)));
}

void ArcButton::setToggleState(bool on, juce::NotificationType n)
{
    if (m_toggleOn == on)
        return;
    m_toggleOn = on;
    if (n != juce::dontSendNotification)
        repaint();
}

void ArcButton::paint(juce::Graphics& g)
{
    auto bodyColour = juce::Colour(0xFF2A2D30);
    if (m_toggleOn)
        bodyColour = bodyColour.interpolatedWith(m_accent, 0.25f);
    if (m_hovered)
        bodyColour = bodyColour.brighter(0.06f);
    if (m_pressed)
        bodyColour = bodyColour.darker(0.04f);

    // 1 — Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.saveState();
    g.addTransform(juce::AffineTransform::translation(1.0f, 2.0f));
    g.fillPath(m_arcPath);
    g.restoreState();

    // 2 — Main body
    g.setColour(bodyColour);
    g.fillPath(m_arcPath);

    // 3 — Subtle surface noise
    {
        g.saveState();
        g.reduceClipRegion(m_arcPath);
        g.setOpacity(0.03f);
        g.drawImage(RenderUtils::getNoiseTexture(),
                    getLocalBounds().toFloat(),
                    juce::RectanglePlacement::fillDestination);
        g.setOpacity(1.0f);
        g.restoreState();
    }

    // 4 — Top highlight gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colours::white.withAlpha(m_pressed ? 0.04f : (m_hovered ? 0.12f : 0.08f)),
        getLocalBounds().toFloat().getTopLeft(),
        juce::Colours::transparentWhite,
        getLocalBounds().toFloat().getCentre(),
        false));
    g.fillPath(m_arcPath);

    // 5 — Bottom shadow gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colours::transparentWhite,
        getLocalBounds().toFloat().getCentre(),
        juce::Colours::black.withAlpha(m_pressed ? 0.22f : (m_hovered ? 0.18f : 0.15f)),
        getLocalBounds().toFloat().getBottomLeft(),
        false));
    g.fillPath(m_arcPath);

    // 6 — Border bevel
    g.setColour(bodyColour.brighter(0.04f));
    g.strokePath(m_arcPath, juce::PathStrokeType(1.0f));

    // 7 — Icon
    if (m_drawIcon)
    {
        g.setColour(juce::Colours::white);
        m_drawIcon(g, getLocalBounds().toFloat().reduced(4));
    }
}

bool ArcButton::hitTest(int x, int y)
{
    return m_arcPath.contains(static_cast<float>(x), static_cast<float>(y));
}

void ArcButton::mouseDown(const juce::MouseEvent&)
{
    m_pressed = true;
    repaint();

    if (m_toggleable)
    {
        m_toggleOn = !m_toggleOn;
        repaint();
    }
    if (onClick)
        onClick();
}

void ArcButton::mouseUp(const juce::MouseEvent&)
{
    m_pressed = false;
    repaint();
}

void ArcButton::mouseEnter(const juce::MouseEvent&)
{
    m_hovered = true;
    repaint();
}

void ArcButton::mouseExit(const juce::MouseEvent&)
{
    m_hovered = false;
    repaint();
}
