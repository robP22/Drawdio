#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include "Resources/ScaledAssetProvider.h"

class SpriteKnob : public juce::Component
{
public:
    SpriteKnob(const ScaledAssetProvider& assets,
               IResourceProvider::ImageId imageId,
               float minV = 0.0f,
               float maxV = 1.0f)
        : m_assets(assets), m_imageId(imageId),
          m_value((minV + maxV) * 0.5f), m_min(minV), m_max(maxV)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setValue(float v)
    {
        v = std::max(m_min, std::min(m_max, v));
        if (m_value != v) { m_value = v; repaint(); }
    }

    float getValue() const { return m_value; }

    void setLinked(bool linked) { if (m_linked != linked) { m_linked = linked; repaint(); } }
    bool isLinked() const { return m_linked; }
    void setLinkRange(float rMin, float rMax)
    {
        rMin = std::clamp(rMin, 0.0f, 1.0f);
        rMax = std::clamp(rMax, 0.0f, 1.0f);
        if (rMax < rMin + 0.05f) rMax = std::min(1.0f, rMin + 0.05f);
        if (rMin > rMax - 0.05f) rMin = std::max(0.0f, rMax - 0.05f);
        if (m_linkMin != rMin || m_linkMax != rMax) { m_linkMin = rMin; m_linkMax = rMax; repaint(); }
    }

    std::function<void(float)> onValueChanged;
    std::function<void(float)> onDragStart;
    std::function<void()> onRightClick;
    std::function<void(float,float)> onLinkRangeChanged;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty()) return;

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

        g.saveState();
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImageTransformed(m_prescaled, t, false);
        g.restoreState();

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

        if (m_linked)
        {
            const float ringRadius = diam * 0.46f;
            const float trackThickness = juce::jmax(2.0f, diam * 0.05f);
            auto drawArc = [&](float startDeg, float endDeg, juce::Colour col, float thickness)
            {
                if (endDeg <= startDeg) return;
                juce::Path p;
                p.addCentredArc(cx, cy, ringRadius, ringRadius, 0.0f,
                                juce::degreesToRadians(startDeg),
                                juce::degreesToRadians(endDeg), true);
                g.setColour(col);
                g.strokePath(p, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            };
            drawArc(-150.0f, 150.0f, juce::Colours::limegreen.withAlpha(0.25f), trackThickness);
            const float minDeg = -150.0f + m_linkMin * 300.0f;
            const float maxDeg = -150.0f + m_linkMax * 300.0f;
            drawArc(minDeg, maxDeg, juce::Colours::limegreen.withAlpha(0.90f), trackThickness);
            const float handleR = juce::jmax(3.0f, diam * 0.07f);
            auto drawHandle = [&](float deg)
            {
                const float rad = juce::degreesToRadians(deg);
                const float hx = cx + std::sin(rad) * ringRadius;
                const float hy = cy - std::cos(rad) * ringRadius;
                g.setColour(juce::Colours::limegreen);
                g.fillEllipse(hx - handleR, hy - handleR, handleR * 2.0f, handleR * 2.0f);
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.drawEllipse(hx - handleR, hy - handleR, handleR * 2.0f, handleR * 2.0f, 1.0f);
            };
            drawHandle(minDeg);
            drawHandle(maxDeg);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (onRightClick) onRightClick();
            return;
        }
        if (m_linked)
        {
            auto bounds = getLocalBounds().toFloat();
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float diam = diamFor(bounds);
            const float ringRadius = diam * 0.46f;
            const float mx = static_cast<float>(e.getPosition().x);
            const float my = static_cast<float>(e.getPosition().y);
            const float dx = mx - cx;
            const float dy = my - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (std::abs(dist - ringRadius) < juce::jmax(8.0f, diam * 0.18f))
            {
                float angleDeg = juce::radiansToDegrees(std::atan2(dx, -dy));
                while (angleDeg < -180.0f) angleDeg += 360.0f;
                while (angleDeg > 180.0f) angleDeg -= 360.0f;
                angleDeg = std::clamp(angleDeg, -150.0f, 150.0f);
                const float minDeg = -150.0f + m_linkMin * 300.0f;
                const float maxDeg = -150.0f + m_linkMax * 300.0f;
                const float distMin = std::abs(angleDeg - minDeg);
                const float distMax = std::abs(angleDeg - maxDeg);
                m_dragMode = (distMin < distMax) ? DragMode::HandleMin : DragMode::HandleMax;
                m_draggingHandle = true;
                return;
            }
        }
        m_dragging = true;
        m_dragMode = DragMode::Knob;
        m_dragStartY = static_cast<float>(e.getPosition().y);
        m_dragStartVal = m_value;
        if (onDragStart) onDragStart(m_value);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (m_draggingHandle)
        {
            auto bounds = getLocalBounds().toFloat();
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float mx = static_cast<float>(e.getPosition().x);
            const float my = static_cast<float>(e.getPosition().y);
            float angleDeg = juce::radiansToDegrees(std::atan2(mx - cx, -(my - cy)));
            while (angleDeg < -180.0f) angleDeg += 360.0f;
            while (angleDeg > 180.0f) angleDeg -= 360.0f;
            angleDeg = std::clamp(angleDeg, -150.0f, 150.0f);
            const float norm = (angleDeg + 150.0f) / 300.0f;
            if (m_dragMode == DragMode::HandleMin)
            {
                const float newMin = std::clamp(norm, 0.0f, m_linkMax - 0.05f);
                if (newMin != m_linkMin) { m_linkMin = newMin; repaint(); if (onLinkRangeChanged) onLinkRangeChanged(m_linkMin, m_linkMax); }
            }
            else if (m_dragMode == DragMode::HandleMax)
            {
                const float newMax = std::clamp(norm, m_linkMin + 0.05f, 1.0f);
                if (newMax != m_linkMax) { m_linkMax = newMax; repaint(); if (onLinkRangeChanged) onLinkRangeChanged(m_linkMin, m_linkMax); }
            }
            return;
        }
        if (!m_dragging) return;
        float deltaY = m_dragStartY - static_cast<float>(e.getPosition().y);
        float raw = m_dragStartVal + deltaY / 200.0f;
        float v = std::max(m_min, std::min(m_max, raw));
        if (m_value != v) { m_value = v; repaint(); if (onValueChanged) onValueChanged(v); }
    }

    void mouseUp(const juce::MouseEvent&) override { m_dragging = false; m_draggingHandle = false; m_dragMode = DragMode::None; }

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
        m_prescaled = m_assets.getScaledImage(
            m_imageId, targetPx, targetPx,
            ScaledAssetProvider::ResamplingPolicy::Continuous);
        m_prescaledSize = m_prescaled.isValid() ? m_prescaled.getWidth() : 0;
    }

    const ScaledAssetProvider& m_assets;
    IResourceProvider::ImageId m_imageId;
    juce::Image m_prescaled;
    int m_prescaledSize = 0;
    float m_value, m_min, m_max;
    float m_dragStartVal = 0.0f;
    float m_dragStartY = 0.0f;
    bool m_dragging = false;
    bool m_draggingHandle = false;
    bool m_linked = false;
    float m_linkMin = 0.0f;
    float m_linkMax = 1.0f;
    enum class DragMode { None, Knob, HandleMin, HandleMax } m_dragMode = DragMode::None;
};
