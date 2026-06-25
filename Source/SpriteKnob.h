#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <functional>

class SpriteKnob : public juce::Component
{
public:
    SpriteKnob(const juce::Image& sprite, float minV = 0.0f, float maxV = 1.0f)
        : m_sprite(sprite), m_min(minV), m_max(maxV), m_value((minV + maxV) * 0.5f)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setValue(float v)
    {
        v = std::max(m_min, std::min(m_max, v));
        if (m_value != v) { m_value = v; repaint(); }
    }

    float getValue() const { return m_value; }

    std::function<void(float)> onValueChanged;
    std::function<void(float)> onDragStart;
    std::function<void()> onRightClick;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (!m_sprite.isValid() || bounds.isEmpty()) return;

        float norm = (m_value - m_min) / (m_max - m_min);
        constexpr float kMaxAngleDegrees = 150.0f;
        float angle = juce::degreesToRadians((norm - 0.5f) * kMaxAngleDegrees * 2.0f);

        float c = std::cos(angle);
        float s_ = std::sin(angle);
        float diam = std::min(bounds.getWidth(), bounds.getHeight());
        float scale = diam / static_cast<float>(m_sprite.getWidth());
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float hw = diam * 0.5f;
        float hh = diam * 0.5f;

        juce::AffineTransform t(
            scale * c,  -scale * s_,  cx - hw * c + hh * s_,
            scale * s_,  scale * c,   cy - hw * s_ - hh * c);

        g.drawImageTransformed(m_sprite, t, false);

        float innerR = diam * 0.48f;
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        for (int step = 0; step <= 10; ++step)
        {
            float v = static_cast<float>(step) * 0.1f;
            float angleDeg = -150.0f + v * 300.0f;
            float rad = juce::degreesToRadians(angleDeg);
            float dx = std::sin(rad);
            float dy = -std::cos(rad);
            bool major = (step == 0 || step == 5 || step == 10);
            float outerR = major ? diam * 0.62f : diam * 0.55f;
            g.drawLine(cx + dx * innerR, cy + dy * innerR,
                       cx + dx * outerR, cy + dy * outerR,
                        major ? 2.0f : 1.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (onRightClick) onRightClick();
            return;
        }
        m_dragging = true;
        m_dragStartY = static_cast<float>(e.getPosition().y);
        m_dragStartVal = m_value;
        if (onDragStart) onDragStart(m_value);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!m_dragging) return;
        float deltaY = m_dragStartY - static_cast<float>(e.getPosition().y);
        float raw = m_dragStartVal + deltaY / 200.0f;
        float v = std::max(m_min, std::min(m_max, raw));
        if (m_value != v) { m_value = v; repaint(); if (onValueChanged) onValueChanged(v); }
    }

    void mouseUp(const juce::MouseEvent&) override { m_dragging = false; }

private:
    const juce::Image& m_sprite;
    float m_value, m_min, m_max;
    float m_dragStartVal = 0.0f;
    float m_dragStartY = 0.0f;
    bool m_dragging = false;
};
