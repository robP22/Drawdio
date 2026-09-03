#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

// Tokoe-style header capsule: [LABEL | value]. The label sits in a darker chip
// on the left; the value slot shows the translucent capsule behind it. A
// "destructive" pill (Reset) keeps its value slot blank and fills it red on
// hover, mirroring the tokoe exit pill's destructive cue.
class HeaderPill : public juce::TextButton
{
public:
    HeaderPill() : juce::TextButton("")
    {
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (hasKeyboardFocus(true))
            giveAwayKeyboardFocus();
        juce::TextButton::mouseDown(event);
    }

    void setPillTexts(const juce::String& label, const juce::String& value)
    {
        m_label = label.toUpperCase();
        m_value = value;
        repaint();
    }

    void setPillValue(const juce::String& value)
    {
        m_value = value;
        repaint();
    }

    void setValueDestructive(bool destructive)
    {
        m_destructiveValue = destructive;
        repaint();
    }

    // Single-pill mode: the whole capsule gets the label-chip backfill with the
    // label text centered (no divider/value slot) — used by the bottom-bar
    // Save/Load/Import buttons.
    void setSolidBackfill(bool solid)
    {
        m_solidBackfill = solid;
        repaint();
    }

    void setFontScales(float labelRatio, float valueRatio)
    {
        m_labelFontScale = labelRatio;
        m_valueFontScale = valueRatio;
        repaint();
    }

    void setPadScale(float padRatio)
    {
        m_padScale = padRatio;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        if (bounds.getWidth() < 2.0f || bounds.getHeight() < 2.0f)
            return;

        const bool hovered = isMouseOver() && isEnabled();
        const bool down = isDown();
        const float h = bounds.getHeight();

        juce::Path body;
        body.addRoundedRectangle(bounds, h * 0.5f);

        auto bodyColour = juce::Colour(0xE6151A1D);
        if (down)
            bodyColour = bodyColour.darker(0.15f);
        g.setColour(bodyColour);
        g.fillPath(body);

        auto lineColour = juce::Colour(0xFF283034);
        if (hovered)
        {
            lineColour = m_destructiveValue ? juce::Colour(0xFFD97968)
                                            : juce::Colour(0xFFD3A84D);
            g.setColour(m_destructiveValue ? juce::Colour(0x3AD97968)
                                           : juce::Colour(0x3AD3A84D));
            g.strokePath(body, juce::PathStrokeType(h * 0.16f));
        }
        g.setColour(lineColour);
        g.strokePath(body, juce::PathStrokeType(1.0f));

        const float pad = h * m_padScale;
        const float labelFontSize = std::max(7.0f, h * m_labelFontScale);
        const float valueFontSize = std::max(8.0f, h * m_valueFontScale);
        juce::Font labelFont { juce::FontOptions(labelFontSize).withStyle("Bold") };
        juce::Font valueFont { juce::FontOptions(valueFontSize) };

        g.saveState();
        g.reduceClipRegion(body);

        if (m_solidBackfill)
        {
            g.setColour(juce::Colour(0x8C0A1014));
            g.fillRect(bounds);
            g.restoreState();

            g.setColour(juce::Colour(0xFFE8F1F5));
            g.setFont(labelFont);
            g.drawText(m_label, bounds, juce::Justification::centred, false);

            if (hasKeyboardFocus(true) && isEnabled())
            {
                g.setColour(juce::Colours::gold.withAlpha(0.7f));
                g.strokePath(body, juce::PathStrokeType(1.5f));
            }
            return;
        }

        const float labelW = juce::GlyphArrangement::getStringWidth(labelFont, m_label) + pad * 2.0f;
        auto chipRect = bounds.withWidth(labelW);

        g.setColour(juce::Colour(0x8C0A1014));
        g.fillRect(chipRect);

        if (m_destructiveValue && hovered && !down)
        {
            g.setColour(juce::Colour(0xE6D97968));
            g.fillRect(bounds.withTrimmedLeft(labelW));
        }
        g.restoreState();

        g.setColour(lineColour);
        g.drawLine(chipRect.getRight(), bounds.getY() + 1.0f,
                   chipRect.getRight(), bounds.getBottom() - 1.0f, 1.0f);

        g.setColour(juce::Colour(0xFFE8F1F5));
        g.setFont(labelFont);
        g.drawText(m_label, chipRect, juce::Justification::centred, false);

        if (!m_value.isEmpty())
        {
            auto valueRect = bounds.withTrimmedLeft(labelW);
            g.setColour(juce::Colour(0xFFE8F1F5));
            g.setFont(valueFont);
            g.drawText(m_value, valueRect, juce::Justification::centred, false);
        }

        if (hasKeyboardFocus(true) && isEnabled())
        {
            g.setColour(juce::Colours::gold.withAlpha(0.7f));
            g.strokePath(body, juce::PathStrokeType(1.5f));
        }
    }

    int getPreferredWidth(float pillHeight) const
    {
        const float pad = pillHeight * m_padScale;
        juce::Font labelFont { juce::FontOptions(std::max(7.0f, pillHeight * m_labelFontScale)).withStyle("Bold") };
        juce::Font valueFont { juce::FontOptions(std::max(8.0f, pillHeight * m_valueFontScale)) };
        const float labelW = juce::GlyphArrangement::getStringWidth(labelFont, m_label) + pad * 2.0f;
        const float valueW = m_value.isEmpty()
            ? pad * 4.0f
            : juce::GlyphArrangement::getStringWidth(valueFont, m_value) + pad * 2.0f;
        return static_cast<int>(std::ceil(labelW + valueW));
    }

private:
    juce::String m_label;
    juce::String m_value;
    bool m_destructiveValue = false;
    bool m_solidBackfill = false;
    float m_labelFontScale = 0.40f;
    float m_valueFontScale = 0.48f;
    float m_padScale = 0.36f;
};
