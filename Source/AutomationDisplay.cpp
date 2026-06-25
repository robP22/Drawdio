#include "AutomationDisplay.h"
#include <algorithm>

AutomationDisplay::AutomationDisplay()
{
    startTimerHz(20);
}

void AutomationDisplay::resized()
{
}

void AutomationDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto graphArea = bounds.reduced(8.0f, 4.0f);
    const float gx = graphArea.getX(), gy = graphArea.getY();
    const float gw = graphArea.getWidth(), gh = graphArea.getHeight();

    // 1. Background
    g.setColour(juce::Colour(0xFF1A1D20));
    g.fillRect(graphArea);

    // 2. Horizontal grid lines (value axis)
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    for (int i = 0; i <= 4; ++i)
    {
        float y = gy + gh * static_cast<float>(i) / 4.0f;
        g.drawLine(gx, y, gx + gw, y, 1.0f);
    }

    // 3. Major bar lines — always 8 bars
    int constexpr totalBars = 8;
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    for (int i = 1; i < totalBars; ++i)
    {
        float x = gx + gw * static_cast<float>(i) / static_cast<float>(totalBars);
        g.drawLine(x, gy, x, gy + gh, 1.5f);
    }

    // 4. Beat subdivisions — 4 per bar across 8 bars = 32
    int constexpr beatTotal = totalBars * 4;
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    for (int i = 1; i < beatTotal; ++i)
    {
        float x = gx + gw * static_cast<float>(i) / static_cast<float>(beatTotal);
        g.drawLine(x, gy + 4.0f, x, gy + gh - 4.0f, 0.5f);
    }

    // Active section bounds
    float activeL = gx + gw * (static_cast<float>(m_sectionStartBar) / static_cast<float>(totalBars));
    float activeW = gw * (static_cast<float>(m_activeBars) / static_cast<float>(totalBars));
    float activeR = activeL + activeW;

    if (!m_envelope.empty())
    {
        // 5. Full envelope curve in dimmed grey across entire width
        {
            juce::Path greyPath;
            bool first = true;
            for (int px = 0; px <= static_cast<int>(gw); px += 2)
            {
                float t = static_cast<float>(px) / gw;
                float v = m_envelope.sample(t);
                float py = gy + gh - v * gh;
                float fx = static_cast<float>(px) + gx;
                if (first) { greyPath.startNewSubPath(fx, py); first = false; }
                else greyPath.lineTo(fx, py);
            }
            g.setColour(juce::Colours::grey.withAlpha(0.30f));
            g.strokePath(greyPath, juce::PathStrokeType(2.0f));
        }

        // 6. Inactive area overlays (darken left/right of active section)
        float leftInactiveW = activeL - gx;
        if (leftInactiveW > 0.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRect(gx, gy, leftInactiveW, gh);
        }
        float rightInactiveX = activeR;
        float rightInactiveW = gx + gw - activeR;
        if (rightInactiveW > 0.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRect(rightInactiveX, gy, rightInactiveW, gh);
        }

        // 7. Active section envelope in limegreen
        {
            juce::Path greenPath;
            bool first = true;
            float startFrac = static_cast<float>(m_sectionStartBar) / static_cast<float>(totalBars);
            float widthFrac = static_cast<float>(m_activeBars) / static_cast<float>(totalBars);
            for (int px = 0; px <= static_cast<int>(activeW); px += 2)
            {
                float localT = static_cast<float>(px) / activeW;
                float envelopeT = std::fmod(startFrac + localT * widthFrac, 1.0f);
                float v = m_envelope.sample(envelopeT);
                float py = gy + gh - v * gh;
                float fx = static_cast<float>(px) + activeL;
                if (first) { greenPath.startNewSubPath(fx, py); first = false; }
                else greenPath.lineTo(fx, py);
            }
            g.setColour(juce::Colours::limegreen);
            g.strokePath(greenPath, juce::PathStrokeType(2.0f));
        }
    }
    else
    {
        // Inactive overlays even when empty
        float leftInactiveW = activeL - gx;
        if (leftInactiveW > 0.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRect(gx, gy, leftInactiveW, gh);
        }
        float rightInactiveX = activeR;
        float rightInactiveW = gx + gw - activeR;
        if (rightInactiveW > 0.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRect(rightInactiveX, gy, rightInactiveW, gh);
        }

        // Flat mid-line
        float midY = gy + gh * 0.5f;
        g.setColour(juce::Colours::limegreen.withAlpha(0.25f));
        g.drawLine(gx, midY, gx + gw, midY, 1.5f);
    }

    // 8. Playhead — positioned within active section
    float phFrac = static_cast<float>(m_sectionStartBar) / static_cast<float>(totalBars)
                 + m_playheadTime * (static_cast<float>(m_activeBars) / static_cast<float>(totalBars));
    float phX = std::max(activeL, std::min(activeR, gx + gw * phFrac));
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawLine(phX, gy, phX, gy + gh, 2.0f);

    // 9. Border
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRect(graphArea, 1.0f);
}

void AutomationDisplay::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds().toFloat();
    auto graphArea = bounds.reduced(8.0f, 4.0f);
    if (!graphArea.contains(e.position))
        return;

    int bar = static_cast<int>((e.position.x - graphArea.getX()) / graphArea.getWidth() * 8.0f);
    bar = std::max(0, std::min(bar, 8 - m_activeBars));
    if (bar != m_sectionStartBar)
    {
        m_sectionStartBar = bar;
        m_needsRepaint = true;
        if (onSectionChanged)
            onSectionChanged(bar);
    }
}
