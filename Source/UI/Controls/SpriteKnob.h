#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <functional>

class SpriteKnob : public juce::Component
{
public:
    SpriteKnob(const juce::Image& sprite, float minV = 0.0f, float maxV = 1.0f)
        : m_sprite(sprite), m_value((minV + maxV) * 0.5f), m_min(minV), m_max(maxV)
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

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        const float deviceScale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const float diam = diamFor(bounds);
        rebuildPrescaledCache(diam, deviceScale);
        if (!m_prescaled.isValid() || m_prescaledSize <= 0) return;

        float norm = (m_value - m_min) / (m_max - m_min);
        constexpr float kMaxAngleDegrees = 150.0f;
        float angle = juce::degreesToRadians((norm - 0.5f) * kMaxAngleDegrees * 2.0f);

        float c = std::cos(angle);
        float s_ = std::sin(angle);
        float scale = diam / static_cast<float>(m_prescaledSize);
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float hw = diam * 0.5f;
        float hh = diam * 0.5f;

        juce::AffineTransform t(
            scale * c,  -scale * s_,  cx - hw * c + hh * s_,
            scale * s_,  scale * c,   cy - hw * s_ - hh * c);

        g.drawImageTransformed(m_prescaled, t, false);

        float innerR = diam * 0.48f;
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        for (int step = 0; step <= 10; ++step)
        {
            float angleDeg = -150.0f + static_cast<float>(step) * 30.0f;
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
    static float diamFor(const juce::Rectangle<float>& bounds)
    {
        return std::min(bounds.getWidth(), bounds.getHeight());
    }

    void rebuildPrescaledCache(float diam, float deviceScale)
    {
        if (diam <= 0.0f || deviceScale <= 0.0f) return;
        const int targetPx = juce::jmax(1, juce::roundToInt(diam * deviceScale));
        if (targetPx == m_prescaledSize && m_prescaled.isValid()) return;
        m_prescaled = m_sprite.rescaled(targetPx, targetPx, juce::Graphics::highResamplingQuality);
        m_prescaledSize = m_prescaled.isValid() ? targetPx : 0;
    }

    const juce::Image& m_sprite;
    juce::Image m_prescaled;
    int m_prescaledSize = 0;
    float m_value, m_min, m_max;
    float m_dragStartVal = 0.0f;
    float m_dragStartY = 0.0f;
    bool m_dragging = false;
};
