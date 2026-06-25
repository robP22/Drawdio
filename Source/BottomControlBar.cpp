#include "BottomControlBar.h"
#include "PluginEditor.h"
#include "GridLayout.h"
#include "PedalDefinition.h"

BottomControlBar::BottomControlBar(DrawdioProcessorEditor* editor, DrawdioProcessor& proc,
                                   const ResourceManager& resources)
    : m_editor(editor), m_processor(proc), m_resources(resources)
{
    addAndMakeVisible(m_barsBtn);
    addAndMakeVisible(m_manualBtn);

    auto& knobImg = m_resources.getImage(ResourceManager::ImageId::PedalKnobImage);
    m_inputKnob = std::make_unique<SpriteKnob>(knobImg, 0.0f, 2.0f);
    m_inputKnob->setValue(m_processor.getDSPProcessor().getInputGain());
    m_inputKnob->onValueChanged = [this](float v) {
        m_processor.getDSPProcessor().setInputGain(v);
    };
    addAndMakeVisible(m_inputKnob.get());

    m_outputKnob = std::make_unique<SpriteKnob>(knobImg, 0.0f, 2.0f);
    m_outputKnob->setValue(m_processor.getDSPProcessor().getOutputGain());
    m_outputKnob->onValueChanged = [this](float v) {
        m_processor.getDSPProcessor().setOutputGain(v);
    };
    addAndMakeVisible(m_outputKnob.get());

    addAndMakeVisible(m_automationDisplay);

    m_barsBtn.onClick = [this]() {
        int next = m_automationDisplay.getBarCount() * 2;
        if (next > 8) next = 1;
        m_automationDisplay.setBarCount(next);
        m_barsBtn.setButtonText(juce::String(next) + " bar" + (next > 1 ? "s" : ""));
        m_processor.setBarCount(next);
        if (m_automationDisplay.onBarCountChanged)
            m_automationDisplay.onBarCountChanged(next);
    };
    m_automationDisplay.onSectionChanged = [this](int start) {
        m_processor.setSectionStart(start);
    };
    m_manualBtn.onClick = [this]() {
        bool next = !m_processor.isManualMode();
        m_processor.setManualMode(next);
        if (next && m_editor)
            m_editor->enterManualMode();
        else if (!next && m_editor)
            m_editor->exitManualMode();
        m_manualBtn.setButtonText(next ? "Canvas" : "Manual");
    };

    auto styleBtn = [](juce::TextButton& b, juce::Colour accent) {
        const auto body = juce::Colour(0xFF2A2D30);
        b.setColour(juce::TextButton::buttonColourId, body);
        b.setColour(juce::TextButton::buttonOnColourId, body.darker(0.06f));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    };
    styleBtn(m_barsBtn, juce::Colour(0xFF8E44AD));
    styleBtn(m_manualBtn, juce::Colour(0xFFE67E22));

    for (int i = 0; i < PedalSlotCount; ++i)
    {
        m_mixerStrips[i] = std::make_unique<MixerStrip>(m_processor, i);
        m_mixerStrips[i]->setPedalName(PedalDefinitions::getDisplayName(m_processor.getPedalSlot(i)));
        addAndMakeVisible(m_mixerStrips[i].get());
    }
}

void BottomControlBar::resized()
{
    const float h = static_cast<float>(getHeight());
    const float pad = h * GridLayout::BottomBar::PadRatio;
    const float usableH = h - pad * 2.0f;
    const float barW = static_cast<float>(getWidth());

    float knobSize = std::min(usableH * GridLayout::BottomBar::KnobMaxSizeRatio, usableH);
    knobSize = std::min(knobSize, 50.0f);
    float knobY = pad + (usableH - knobSize) * 0.5f;

    // Input knob: far left
    m_inputKnob->setBounds(static_cast<int>(pad), static_cast<int>(knobY),
                           static_cast<int>(knobSize), static_cast<int>(knobSize));

    // Button stack: 2 buttons, evenly spaced
    const float btnW = std::min(usableH * GridLayout::BottomBar::BtnWidthRatio,
                                GridLayout::BottomBar::BtnMaxWidth);
    const float btnH = std::min(usableH * GridLayout::BottomBar::BtnHeightRatio,
                                GridLayout::BottomBar::BtnMaxHeight);
    const float btnGap = (usableH - btnH * 2.0f) / 1.0f;
    float btnX = pad + knobSize + pad;

    juce::TextButton* btns[2] = {&m_barsBtn, &m_manualBtn};
    for (int i = 0; i < 2; ++i)
        btns[i]->setBounds(static_cast<int>(btnX), static_cast<int>(pad + (btnH + btnGap) * i),
                           static_cast<int>(btnW), static_cast<int>(btnH));

    // Automation: fills space between buttons and mixer strips
    float autoX = btnX + btnW + pad;

    // Mixer strips: computed from ratios
    const float stripW = usableH * GridLayout::BottomBar::StripWidthRatio;
    const float stripGap = usableH * GridLayout::BottomBar::StripGapRatio;
    const float stripTotal = usableH * GridLayout::BottomBar::StripTotalRatio;

    // Output knob: far right
    float outKnobX = barW - pad - knobSize;
    float stripStartX = outKnobX - pad - stripTotal;

    m_automationDisplay.setBounds(static_cast<int>(autoX), static_cast<int>(pad),
                                   static_cast<int>(stripStartX - autoX - pad),
                                   static_cast<int>(usableH));

    for (int i = 0; i < PedalSlotCount; ++i)
        m_mixerStrips[i]->setBounds(static_cast<int>(stripStartX + (stripW + stripGap) * i),
                                     static_cast<int>(pad),
                                     static_cast<int>(stripW),
                                     static_cast<int>(usableH));

    m_outputKnob->setBounds(static_cast<int>(outKnobX), static_cast<int>(knobY),
                            static_cast<int>(knobSize), static_cast<int>(knobSize));
}

void BottomControlBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();
    const float pad = h * GridLayout::BottomBar::PadRatio;
    const float usableH = h - pad * 2.0f;
    const float knobSize = std::min(usableH * GridLayout::BottomBar::KnobMaxSizeRatio, usableH);
    const float btnW = std::min(usableH * GridLayout::BottomBar::BtnWidthRatio,
                                GridLayout::BottomBar::BtnMaxWidth);

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRect(b);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    if (m_inputKnob)
    {
        auto sb = m_inputKnob->getBounds();
        g.drawText("In", sb.getX(), static_cast<int>(pad), sb.getWidth(),
                   sb.getY() - static_cast<int>(pad), juce::Justification::centredBottom, false);
    }
    if (m_outputKnob)
    {
        auto sb = m_outputKnob->getBounds();
        g.drawText("Out", sb.getX(), static_cast<int>(pad), sb.getWidth(),
                   sb.getY() - static_cast<int>(pad), juce::Justification::centredBottom, false);
    }

    const float dividerAlpha = 0.08f;
    g.setColour(juce::Colours::white.withAlpha(dividerAlpha));

    float x = pad + knobSize + pad;
    g.drawLine(x, pad, x, h - pad, 1.0f);

    x += btnW + pad;
    g.drawLine(x, pad, x, h - pad, 1.0f);

    float outKnobX = b.getWidth() - pad - knobSize;
    float stripStart = outKnobX - pad - (usableH * GridLayout::BottomBar::StripTotalRatio);
    g.drawLine(stripStart, pad, stripStart, h - pad, 1.0f);

    g.drawLine(outKnobX - pad, pad, outKnobX - pad, h - pad, 1.0f);
}

void BottomControlBar::syncPedalNames()
{
    for (int i = 0; i < PedalSlotCount; ++i)
        m_mixerStrips[i]->setPedalName(PedalDefinitions::getDisplayName(m_processor.getPedalSlot(i)));
}

void BottomControlBar::syncGainKnobs()
{
    m_inputKnob->setValue(m_processor.getDSPProcessor().getInputGain());
    m_outputKnob->setValue(m_processor.getDSPProcessor().getOutputGain());
}
