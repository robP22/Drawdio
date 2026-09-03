#include "PedalboardHeader.h"
#include "Core/EditorDesignMetrics.h"
#include "UI/EditorLayout.h"

PedalboardHeader::PedalboardHeader()
{
    setInterceptsMouseClicks(false, true);
    addAndMakeVisible(m_resetPill);
    addAndMakeVisible(m_modePill);

    m_resetPill.setPillTexts("PEDALS", "Reset");
    m_resetPill.setValueDestructive(true);
    m_modePill.setPillTexts("MODE", "Canvas");

    m_resetPill.onClick = [this]() { if (onReset) onReset(); };
    m_modePill.onClick = [this]() { if (onModeToggle) onModeToggle(); };
}

void PedalboardHeader::resized()
{
    const float h = static_cast<float>(getHeight());
    const float pad = h * EditorDesignMetrics::PedalboardHeader::PadRatio;
    const float usableH = h - pad * 2.0f;
    const float scale = EditorLayout::scaleFromHeight(h, EditorDesignMetrics::PedalboardHeader::HeightRatio);
    const float pillH = std::min(usableH, EditorDesignMetrics::PedalboardHeader::PillMaxHeight * scale);
    const float gap = h * EditorDesignMetrics::PedalboardHeader::BtnGapRatio;

    const float modeW = static_cast<float>(m_modePill.getPreferredWidth(pillH));
    const float resetW = static_cast<float>(m_resetPill.getPreferredWidth(pillH));
    const float groupW = modeW + resetW + gap;
    const float x0 = (static_cast<float>(getWidth()) - groupW) * 0.5f;
    const float y = std::max(0.0f, m_buttonCenterY - pillH * 0.5f
        + pillH * EditorDesignMetrics::PedalboardHeader::BtnCenterShiftRatio);

    const int yPx = juce::roundToInt(y);
    const int pillHPx = juce::roundToInt(pillH);
    const int resetWPx = juce::roundToInt(resetW);
    const int modeWPx = juce::roundToInt(modeW);
    const int resetXPx = juce::roundToInt(x0);
    const int modeXPx = resetXPx + resetWPx + juce::roundToInt(gap);
    m_resetPill.setBounds(resetXPx, yPx, resetWPx, pillHPx);
    m_modePill.setBounds(modeXPx, yPx, modeWPx, pillHPx);
}

void PedalboardHeader::updateModeButton(bool manual)
{
    m_modePill.setPillValue(manual ? "Manual" : "Canvas");
}
