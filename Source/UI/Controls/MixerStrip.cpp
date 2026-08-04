#include "UI/Controls/MixerStrip.h"
#include "GridLayout.h"


MixerStrip::MixerStrip(IMixerStripModel& model, int slotIdx)
    : m_model(model), m_slotIndex(slotIdx)
{
    startTimerHz(20);
}

void MixerStrip::resized()
{
    const float h = getHeight();
    const float labelH = h * GridLayout::Mixer::NameLabelHeightRatio;
    auto b = getLocalBounds().toFloat().withTrimmedTop(labelH);

    float meterW = h * GridLayout::Mixer::MeterWidthRatio;
    float gap = h * GridLayout::Mixer::MeterTrackGapRatio;
    float trackW = h * GridLayout::Mixer::TrackWidthRatio;
    float blockW = meterW + gap + trackW;
    float sidePad = std::max(0.0f, (b.getWidth() - blockW) * 0.5f);
    auto block = b.reduced(sidePad, 0.0f);
    m_meterBounds = block.removeFromLeft(meterW).reduced(1.0f);
    m_sliderBounds = block;
}

void MixerStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float h = bounds.getHeight();
    const float labelH = h * GridLayout::Mixer::NameLabelHeightRatio;

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText(m_pedalName, bounds.withHeight(labelH),
               juce::Justification::centred, false);

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRect(bounds, 1.0f);

    bool active = (m_model.getPedalSlot(m_slotIndex) != DspModuleType::BYPASS);

    // Meter
    g.setColour(juce::Colour(0xFF1A1D20));
    g.fillRect(m_meterBounds);
    if (active)
    {
        float fillH = m_meterBounds.getHeight() * juce::jmap(juce::Decibels::gainToDecibels(m_displayPeak), -32.0f, 6.0f, 0.0f, 1.0f);
        auto fillRect = m_meterBounds.withTop(m_meterBounds.getBottom() - fillH);
        juce::ColourGradient grad(
            juce::Colours::green, fillRect.getBottomLeft(),
            juce::Colours::red, fillRect.getTopLeft(), false);
        g.setGradientFill(grad);
        g.fillRect(fillRect);
    }

    // Slider track with groove
    float trackW = h * GridLayout::Mixer::TrackWidthRatio;
    float trackX = m_sliderBounds.getCentreX() - trackW * 0.5f;
    auto track = juce::Rectangle<float>(trackX, m_sliderBounds.getY(), trackW, m_sliderBounds.getHeight());
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(track);
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.drawLine(track.getX(), track.getY(), track.getX(), track.getBottom(), 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawLine(track.getRight(), track.getY(), track.getRight(), track.getBottom(), 1.0f);

    if (active)
    {
        float thumbW = h * GridLayout::Mixer::ThumbWidthRatio;
        float thumbH = h * GridLayout::Mixer::ThumbHeightRatio;
        float thumbY = m_sliderBounds.getY() + m_sliderBounds.getHeight() * (1.0f - juce::jmap(juce::Decibels::gainToDecibels(m_displayGain), -32.0f, 6.0f, 0.0f, 1.0f));
        auto thumbRect = juce::Rectangle<float>(m_sliderBounds.getCentreX() - thumbW * 0.5f,
                                                  thumbY - thumbH * 0.5f, thumbW, thumbH);

        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(thumbRect.translated(1.0f, 1.0f));
        g.setColour(juce::Colour(0xFF707070));
        g.fillRect(thumbRect);
        g.setColour(juce::Colour(0xFF9A9A9A));
        g.fillRect(thumbRect.withHeight(thumbRect.getHeight() * 0.4f));
    }
}

void MixerStrip::mouseDown(const juce::MouseEvent& e)
{
    if (m_model.getPedalSlot(m_slotIndex) == DspModuleType::BYPASS) return;
    const float h = getHeight();
    float expand = h * GridLayout::Mixer::SliderHitExpandRatio;
    if (m_sliderBounds.expanded(expand).contains(e.position))
    {
        m_dragging = true;
        float frac = 1.0f - (e.position.y - m_sliderBounds.getY()) / m_sliderBounds.getHeight();
        m_displayGain = juce::Decibels::decibelsToGain(juce::jmap(frac, 0.0f, 1.0f, -32.0f, 6.0f));
        m_model.setPedalGain(m_slotIndex, m_displayGain);
        repaint();
    }
}

void MixerStrip::mouseDrag(const juce::MouseEvent& e)
{
    if (!m_dragging) return;
    float frac = 1.0f - (e.position.y - m_sliderBounds.getY()) / m_sliderBounds.getHeight();
    m_displayGain = juce::Decibels::decibelsToGain(juce::jmap(frac, 0.0f, 1.0f, -32.0f, 6.0f));
    m_model.setPedalGain(m_slotIndex, m_displayGain);
    repaint();
}

void MixerStrip::mouseUp(const juce::MouseEvent&)
{
    m_dragging = false;
}

void MixerStrip::timerCallback()
{
    float peak = m_model.getPedalPeak(m_slotIndex);
    float gain = m_model.getPedalGain(m_slotIndex);
    if (std::abs(peak - m_displayPeak) > 0.001f || std::abs(gain - m_displayGain) > 0.001f)
    {
        m_displayPeak = m_displayPeak * 0.85f + peak * 0.15f;
        m_displayGain = gain;
        repaint();
    }
    else
    {
        m_displayPeak = m_displayPeak * 0.85f;
        if (m_displayPeak > peak && m_displayPeak > 0.002f) repaint();
    }
}
