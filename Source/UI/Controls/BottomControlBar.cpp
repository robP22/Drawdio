#include "BottomControlBar.h"
#include "Core/EditorDesignMetrics.h"
#include "UI/EditorLayout.h"
#include "State/PedalDefinition.h"

BottomControlBar::BottomControlBar(const ScaledAssetProvider& assets,
                                   Actions actions)
    : m_assets(assets), m_actions(std::move(actions))
{
    addAndMakeVisible(m_barsPill);
    addAndMakeVisible(m_presetPill);
    addAndMakeVisible(m_imagePill);

    m_inputKnob = std::make_unique<SpriteKnob>(m_assets, IResourceProvider::ImageId::PedalKnobImage, 0.0f, 2.0f);
    m_inputKnob->setValue(1.0f);
    m_inputKnob->onValueChanged = [this](float v) {
        if (m_actions.setInputGain) m_actions.setInputGain(v);
    };
    addAndMakeVisible(m_inputKnob.get());

    m_outputKnob = std::make_unique<SpriteKnob>(m_assets, IResourceProvider::ImageId::PedalKnobImage, 0.0f, 2.0f);
    m_outputKnob->setValue(1.0f);
    m_outputKnob->onValueChanged = [this](float v) {
        if (m_actions.setOutputGain) m_actions.setOutputGain(v);
    };
    addAndMakeVisible(m_outputKnob.get());

    addAndMakeVisible(m_automationDisplay);

    m_barsPill.setPillTexts("LENGTH", "1 bar");
    m_barsPill.setFontScales(0.55f, 0.60f);
    m_barsPill.setPadScale(0.25f);
    m_barsPill.onClick = [this]() {
        int next = m_automationDisplay.getBarCount() * 2;
        if (next > 8) next = 1;
        m_automationDisplay.setBarCount(next);
        m_barsPill.setPillValue(juce::String(next) + " bar" + (next > 1 ? "s" : ""));
        if (m_actions.setBarCount) m_actions.setBarCount(next);
        if (m_automationDisplay.onBarCountChanged)
            m_automationDisplay.onBarCountChanged(next);
    };

    auto styleSplitPill = [this](SplitPill& pill) {
        pill.setFontScales(0.55f, 0.60f);
        pill.setPadScale(0.25f);
    };
    m_presetPill.setTexts("SAVE", "IMPORT");
    m_imagePill.setTexts("IMPORT", "EXPORT");
    m_presetPill.setTooltips("Save preset to .drawdio", "Import preset from .drawdio");
    m_imagePill.setTooltips("Import image to canvas (dithered to palette)", "Export canvas to PNG");
    styleSplitPill(m_presetPill);
    styleSplitPill(m_imagePill);

    m_automationDisplay.onSectionChanged = [this](int start) {
        if (m_actions.setSectionStart) m_actions.setSectionStart(start);
    };
    m_presetPill.onLeftClick = [this]() {
        if (onPresetSave)
            onPresetSave();
    };
    m_presetPill.onRightClick = [this]() {
        if (onPresetImport)
            onPresetImport();
    };
    m_imagePill.onLeftClick = [this]() {
        if (onImageImport)
            onImageImport();
    };
    m_imagePill.onRightClick = [this]() {
        if (onImageExport)
            onImageExport();
    };

    for (int i = 0; i < PedalSlotCount; ++i)
    {
        MixerStrip::Actions actions;
        actions.setGain = [this](int slot, float gain)
        {
            if (m_actions.setPedalGain) m_actions.setPedalGain(slot, gain);
        };
        MixerStripViewState state;
        state.type = DspModuleType::BYPASS;
        m_mixerStrips[i] = std::make_unique<MixerStrip>(i, state, std::move(actions));
        m_mixerStrips[i]->setPedalName(PedalDefinitions::getDisplayName(state.type));
        addAndMakeVisible(m_mixerStrips[i].get());
    }
}

void BottomControlBar::resized()
{
    const float h = static_cast<float>(getHeight());
    const float pad = h * EditorDesignMetrics::BottomBar::PadRatio;
    const float usableH = h - pad * 2.0f;
    const float barW = static_cast<float>(getWidth());
    const float scale = EditorLayout::scaleFromHeight(h, EditorDesignMetrics::BottomBar::HeightRatio);

    const int padPx = juce::roundToInt(pad);
    const int knobSizePx = juce::roundToInt(std::min(
        EditorLayout::scaledCap(usableH, EditorDesignMetrics::BottomBar::KnobMaxSizeRatio, 50.0f, scale), usableH));
    const int knobYPx = juce::roundToInt(pad + (usableH - static_cast<float>(knobSizePx)) * 0.5f);

    m_inputKnob->setBounds(padPx, knobYPx, knobSizePx, knobSizePx);

    // Button stack: Length pill, then Preset split + Image split
    const float btnW = EditorLayout::scaledCap(usableH, EditorDesignMetrics::BottomBar::BtnWidthRatio,
                                               EditorDesignMetrics::BottomBar::BtnMaxWidth, scale);
    const float btnH = EditorLayout::scaledCap(usableH, EditorDesignMetrics::BottomBar::BtnHeightRatio,
                                               EditorDesignMetrics::BottomBar::BtnMaxHeight, scale);
    const int btnWPx = juce::roundToInt(btnW);
    const int btnHPx = juce::roundToInt(btnH);
    const int labelGapPx = juce::roundToInt(usableH * EditorDesignMetrics::BottomBar::BtnLabelGapRatio);
    const int groupGapPx = juce::roundToInt(usableH * EditorDesignMetrics::BottomBar::BtnGroupGapRatio);
    const int stackH = btnHPx * 3 + labelGapPx * 2;
    const int btnXPx = juce::roundToInt(pad + static_cast<float>(knobSizePx) + pad + pad * 0.5f);
    int btnYPx = juce::roundToInt(pad + (usableH - static_cast<float>(stackH)) * 0.5f
                                   + btnH * EditorDesignMetrics::BottomBar::BtnVerticalShiftRatio);

    m_barsPill.setBounds(btnXPx, btnYPx, btnWPx, btnHPx);
    btnYPx += btnHPx + labelGapPx;
    m_presetPill.setBounds(btnXPx, btnYPx, btnWPx, btnHPx);
    btnYPx += btnHPx + labelGapPx;
    m_imagePill.setBounds(btnXPx, btnYPx, btnWPx, btnHPx);

    // Automation: fills space between buttons and mixer strips
    const int autoXPx = btnXPx + btnWPx + padPx;

    // Mixer strips: computed from ratios
    const float stripW = usableH * EditorDesignMetrics::BottomBar::StripWidthRatio;
    const float stripGap = usableH * EditorDesignMetrics::BottomBar::StripGapRatio;
    const float stripTotal = usableH * EditorDesignMetrics::BottomBar::StripTotalRatio;

    // Output knob: far right
    const int stripWPx = juce::roundToInt(stripW);
    const int stripGapPx = juce::roundToInt(stripGap);
    const int stripStartPx = juce::roundToInt(barW - pad - static_cast<float>(knobSizePx) - pad - stripTotal);
    const int outKnobXPx = juce::roundToInt(barW - pad - static_cast<float>(knobSizePx));

    m_automationDisplay.setBounds(autoXPx, padPx,
                                   juce::jmax(0, stripStartPx - autoXPx - padPx),
                                   juce::roundToInt(usableH));

    for (int i = 0; i < PedalSlotCount; ++i)
        m_mixerStrips[i]->setBounds(stripStartPx + (stripWPx + stripGapPx) * i,
                                     padPx, stripWPx, juce::roundToInt(usableH));

    m_outputKnob->setBounds(outKnobXPx, knobYPx, knobSizePx, knobSizePx);
}

void BottomControlBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int wPx = juce::jmax(1, juce::roundToInt(b.getWidth() * scale));
    const int hPx = juce::jmax(1, juce::roundToInt(b.getHeight() * scale));
    const auto bg = m_assets.getScaledImage(IResourceProvider::ImageId::BottomControlBarBg, wPx, hPx,
                                            ScaledAssetProvider::ResamplingPolicy::Continuous);
    if (bg.isValid())
    {
        g.drawImageWithin(bg, 0, 0, b.getWidth(), b.getHeight(),
                          juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::onlyReduceInSize, false);
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(b);
    }

    const float h = b.getHeight();
    const float pad = h * EditorDesignMetrics::BottomBar::PadRatio;
    const float usableH = h - pad * 2.0f;
    const float knobSize = std::min(usableH * EditorDesignMetrics::BottomBar::KnobMaxSizeRatio, usableH);
    const float btnW = std::min(usableH * EditorDesignMetrics::BottomBar::BtnWidthRatio,
                                EditorDesignMetrics::BottomBar::BtnMaxWidth);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(juce::jlimit(10.0f, 14.0f, usableH * 0.12f))));
    if (m_inputKnob)
    {
        auto sb = m_inputKnob->getBounds();
        g.drawText("In", sb.getX(), static_cast<int>(pad), sb.getWidth(),
                   sb.getY() - static_cast<int>(pad), juce::Justification::centredTop, false);
    }
    if (m_outputKnob)
    {
        auto sb = m_outputKnob->getBounds();
        g.drawText("Out", sb.getX(), static_cast<int>(pad), sb.getWidth(),
                   sb.getY() - static_cast<int>(pad), juce::Justification::centredTop, false);
    }

    const auto drawCaptionLabel = [&](const juce::Component& b, const char* text, int y, int height)
    {
        const auto labelRect = juce::Rectangle<int>(b.getX(), y, b.getWidth(), height).reduced(1);
        if (labelRect.getHeight() < 6)
            return;
        g.setColour(juce::Colours::white.withAlpha(0.80f));
        g.setFont(juce::Font(juce::FontOptions(juce::jlimit(10.0f, 12.5f, usableH * 0.125f))));
        g.drawText(text, labelRect, juce::Justification::centred, false);
    };
    const int labelH = juce::jlimit(12, 18, static_cast<int>(usableH * 0.16f));
    drawCaptionLabel(m_barsPill, "Automation", m_barsPill.getY() - labelH, labelH);
    drawCaptionLabel(m_presetPill, "Preset", m_barsPill.getBottom(),
                     m_presetPill.getY() - m_barsPill.getBottom() - 1);
    drawCaptionLabel(m_imagePill, "Image", m_presetPill.getBottom(),
                     m_imagePill.getY() - m_presetPill.getBottom() - 1);

    const float dividerAlpha = 0.08f;
    g.setColour(juce::Colours::white.withAlpha(dividerAlpha));

    float x = pad + knobSize + pad;
    g.drawLine(x, pad, x, h - pad, 1.0f);

    x += btnW + pad;
    g.drawLine(x, pad, x, h - pad, 1.0f);

    float outKnobX = b.getWidth() - pad - knobSize;
    float stripStart = outKnobX - pad - (usableH * EditorDesignMetrics::BottomBar::StripTotalRatio);
    g.drawLine(stripStart, pad, stripStart, h - pad, 1.0f);

    g.drawLine(outKnobX - pad, pad, outKnobX - pad, h - pad, 1.0f);
}

void BottomControlBar::syncPedalNames()
{
    for (int i = 0; i < PedalSlotCount; ++i)
        m_mixerStrips[i]->setPedalName(PedalDefinitions::getDisplayName(m_pedalTypes[static_cast<size_t>(i)]));
}

void BottomControlBar::tick()
{
    syncPedalNames();
    m_automationDisplay.tick();
    for (auto& strip : m_mixerStrips)
        strip->tick();
}

void BottomControlBar::setManualMode(bool manual)
{
    m_automationDisplay.setManualMode(manual);
}

void BottomControlBar::setViewState(const EditorUiSnapshot& state)
{
    m_inputKnob->setValue(state.inputGain);
    m_outputKnob->setValue(state.outputGain);
    m_automationDisplay.setBarCount(state.barCount);
    updateBarsButton(state.barCount);
    m_automationDisplay.setSectionStart(state.sectionStartBar);

    for (int i = 0; i < PedalSlotCount; ++i)
    {
        const auto& pedal = state.pedals[static_cast<size_t>(i)];
        m_pedalTypes[static_cast<size_t>(i)] = pedal.type;
        m_mixerStrips[static_cast<size_t>(i)]->setPedalName(PedalDefinitions::getDisplayName(pedal.type));
        MixerStripViewState mixerState;
        mixerState.type = pedal.type;
        mixerState.peak = pedal.peak;
        mixerState.gain = pedal.gain;
        m_mixerStrips[static_cast<size_t>(i)]->setViewState(mixerState);
    }
}

void BottomControlBar::setPedalPeak(int slot, float peak)
{
    if (slot >= 0 && slot < PedalSlotCount)
        m_mixerStrips[static_cast<size_t>(slot)]->setPeak(peak);
}
