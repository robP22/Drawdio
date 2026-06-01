#include "PedalComponent.h"
#include "PluginProcessor.h"

namespace
{
class PedalKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                             static_cast<float>(y),
                                             static_cast<float>(width),
                                             static_cast<float>(height)).reduced(7.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle
            + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.fillEllipse(bounds.translated(0.0f, 3.0f));

        juce::ColourGradient body(juce::Colour(0xFFB8BEC0),
                                  centre.x - radius, centre.y - radius,
                                  juce::Colour(0xFF3C4144),
                                  centre.x + radius, centre.y + radius,
                                  false);
        body.addColour(0.45, juce::Colour(0xFF777E82));
        g.setGradientFill(body);
        g.fillEllipse(bounds);

        g.setColour(juce::Colours::black.withAlpha(0.58f));
        g.drawEllipse(bounds, 1.4f);

        auto cap = bounds.reduced(radius * 0.25f);
        juce::ColourGradient capGradient(juce::Colour(0xFFD9DEE0),
                                         cap.getX(), cap.getY(),
                                         juce::Colour(0xFF5B6265),
                                         cap.getRight(), cap.getBottom(),
                                         false);
        g.setGradientFill(capGradient);
        g.fillEllipse(cap);

        g.setColour(juce::Colours::white.withAlpha(0.34f));
        g.fillEllipse(cap.withSizeKeepingCentre(cap.getWidth() * 0.46f,
                                                cap.getHeight() * 0.22f)
                          .translated(-cap.getWidth() * 0.12f, -cap.getHeight() * 0.18f));

        juce::Path indicator;
        const auto lineStart = centre.getPointOnCircumference(radius * 0.18f, angle);
        const auto lineEnd = centre.getPointOnCircumference(radius * 0.72f, angle);
        indicator.startNewSubPath(lineStart);
        indicator.lineTo(lineEnd);

        g.setColour(juce::Colour(0xFF101214));
        g.strokePath(indicator, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawEllipse(bounds.reduced(2.0f), 1.0f);
    }
};

PedalKnobLookAndFeel& knobLookAndFeel()
{
    static PedalKnobLookAndFeel lookAndFeel;
    return lookAndFeel;
}

juce::Colour skinColourForSlot(int slot)
{
    static constexpr uint32_t colours[] {
        0xFF3B5A74, 0xFF6E3E49, 0xFF4D6846,
        0xFF6B603A, 0xFF584E75, 0xFF5E6266
    };
    return juce::Colour(colours[static_cast<size_t>(slot % 6)]);
}
}

const char* PedalComponent::typeName(DspModuleType t)
{
    switch (t)
    {
        case DspModuleType::BYPASS:                   return "Bypass";
        case DspModuleType::WAVESHAPER_DISTORTION:    return "Waveshaper Dist.";
        case DspModuleType::MODULATED_DELAY_LINE:     return "Mod. Delay";
        case DspModuleType::BIQUAD_FILTER:            return "Biquad Filter";
        case DspModuleType::DYNAMIC_RING_BUFFER:       return "Dyn. Ring Buffer";
        case DspModuleType::PITCH_SHIFTER_GRANULAR:   return "Pitch Shifter";
        case DspModuleType::ENVELOPE_VCA_COMPRESSOR:  return "VCA Compressor";
        case DspModuleType::PITCH_DETECTOR_OSCILLATOR:return "Pitch Detector";
        case DspModuleType::DIFFUSED_DELAY_NETWORK:   return "Diff. Delay Net";
        case DspModuleType::ALLPASS_FILTER_CASCADE:   return "Allpass Cascade";
        case DspModuleType::FREQUENCY_SHIFTER:        return "Freq. Shifter";
        case DspModuleType::MATHEMATICAL_WAVEFOLDER:  return "Wavefolder";
        case DspModuleType::SAMPLE_RATE_DEGRADER:     return "Sample Rate Deg.";
        case DspModuleType::FORMANT_VOCAL_SHIFTER:    return "Formant Shifter";
        case DspModuleType::TAPE_STOP_REVERSE_ECHO:   return "Tape Stop Echo";
        case DspModuleType::SIMPLE_DELAY:             return "Simple Delay";
        case DspModuleType::PLATE_REVERB:             return "Plate Reverb";
        case DspModuleType::SOFT_DISTORTION:          return "Soft Distortion";
        case DspModuleType::GRANULAR_DELAY:           return "Gran. Delay";
        default: return "Unknown";
    }
}

juce::String PedalComponent::knobLabel(DspModuleType t, int knobIdx)
{
    if (knobIdx == 0) return "Wet";
    if (knobIdx == 1) return "Dry";
    if (knobIdx == 2) return "Vol";

    switch (t)
    {
        case DspModuleType::WAVESHAPER_DISTORTION:     return "Drive";
        case DspModuleType::MODULATED_DELAY_LINE:      return "Rate";
        case DspModuleType::BIQUAD_FILTER:             return "Cutoff";
        case DspModuleType::DYNAMIC_RING_BUFFER:        return "Size";
        case DspModuleType::PITCH_SHIFTER_GRANULAR:    return "Pitch";
        case DspModuleType::ENVELOPE_VCA_COMPRESSOR:   return "Thresh";
        case DspModuleType::PITCH_DETECTOR_OSCILLATOR: return "Octave";
        case DspModuleType::DIFFUSED_DELAY_NETWORK:    return "Decay";
        case DspModuleType::ALLPASS_FILTER_CASCADE:    return "Coeff";
        case DspModuleType::FREQUENCY_SHIFTER:         return "Shift";
        case DspModuleType::MATHEMATICAL_WAVEFOLDER:   return "Fold";
        case DspModuleType::SAMPLE_RATE_DEGRADER:      return "Bits";
        case DspModuleType::FORMANT_VOCAL_SHIFTER:     return "Formant";
        case DspModuleType::TAPE_STOP_REVERSE_ECHO:    return "Brake";
        case DspModuleType::SIMPLE_DELAY:              return "Time";
        case DspModuleType::PLATE_REVERB:              return "Decay";
        case DspModuleType::SOFT_DISTORTION:           return "Drive";
        case DspModuleType::GRANULAR_DELAY:            return "Pitch";
        default: return "Param";
    }
}

PedalComponent::PedalComponent(DrawdioProcessor& processor,
                               int slotIndex,
                               DspModuleType initialType)
    : audioProcessor(processor),
      m_slotIndex(slotIndex),
      m_currentType(initialType)
{
    for (auto& knob : m_knobs)
        initKnob(knob);
}

PedalComponent::~PedalComponent()
{
    for (auto& knob : m_knobs)
        knob.setLookAndFeel(nullptr);
}

void PedalComponent::initKnob(juce::Slider& knob)
{
    addAndMakeVisible(knob);
    knob.setLookAndFeel(&knobLookAndFeel());
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.18f,
                             juce::MathConstants<float>::pi * 2.82f,
                             true);
    knob.setRange(0.0, 1.0, 0.01);
    knob.setDoubleClickReturnValue(true, 0.5);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    knob.onValueChange = [this, &knob]()
    {
        int knobIdx = -1;
        for (int i = 0; i < 4; ++i)
        {
            if (&m_knobs[i] == &knob)
            {
                knobIdx = i;
                break;
            }
        }

        if (knobIdx >= 0)
            audioProcessor.getDSPProcessor().updateParameter(m_slotIndex,
                                                             knobIdx,
                                                             static_cast<float>(knob.getValue()));
    };
}

void PedalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto body = bounds.reduced(5.0f, 9.0f);
    body.removeFromTop(6.0f);

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(body.translated(2.0f, 5.0f), 13.0f);

    juce::ColourGradient sideGradient(skinColourForSlot(m_slotIndex).darker(0.65f),
                                      body.getX(), body.getY(),
                                      juce::Colour(0xFF0F1113),
                                      body.getX(), body.getBottom(),
                                      false);
    g.setGradientFill(sideGradient);
    g.fillRoundedRectangle(body, 13.0f);

    auto face = body.reduced(7.0f, 6.0f);
    juce::Colour skin = skinColourForSlot(m_slotIndex);
    juce::ColourGradient faceGradient(skin.brighter(0.08f), face.getX(), face.getY(),
                                      skin.darker(0.52f), face.getX(), face.getBottom(),
                                      false);
    g.setGradientFill(faceGradient);
    g.fillRoundedRectangle(face, 10.0f);

    juce::Random random(0xD0A0 + m_slotIndex);
    for (int i = 0; i < 90; ++i)
    {
        const auto x = face.getX() + random.nextFloat() * face.getWidth();
        const auto y = face.getY() + random.nextFloat() * face.getHeight();
        g.setColour(juce::Colours::white.withAlpha(random.nextFloat() * 0.035f));
        g.fillRect(x, y, 1.0f + random.nextFloat() * 2.0f, 0.7f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(face.reduced(1.0f), 9.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.56f));
    g.drawRoundedRectangle(body, 13.0f, 1.8f);

    auto labelTop = face.removeFromTop(24.0f).reduced(8.0f, 2.0f);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colours::white.withAlpha(0.76f));
    g.drawFittedText("DRAWDIO " + juce::String(m_slotIndex + 1),
                     labelTop.toNearestInt(),
                     juce::Justification::centred,
                     1);

    auto lcdArea = body.withTrimmedTop(body.getHeight() * 0.67f).reduced(18.0f, 10.0f);
    g.setColour(juce::Colours::black.withAlpha(0.60f));
    g.fillRoundedRectangle(lcdArea.expanded(3.0f), 6.0f);
    g.setColour(juce::Colour(0xFF050707));
    g.fillRoundedRectangle(lcdArea, 5.0f);

    juce::ColourGradient lens(juce::Colour(0xFF233034).withAlpha(0.70f),
                              lcdArea.getX(), lcdArea.getY(),
                              juce::Colour(0xFF070909).withAlpha(0.96f),
                              lcdArea.getRight(), lcdArea.getBottom(),
                              false);
    g.setGradientFill(lens);
    g.fillRoundedRectangle(lcdArea.reduced(2.0f), 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.88f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawFittedText(typeName(m_currentType),
                     lcdArea.reduced(5.0f, 2.0f).toNearestInt(),
                     juce::Justification::centred,
                     1);

    auto inJack = getInputJackPos() - getPosition().toFloat();
    auto outJack = getOutputJackPos() - getPosition().toFloat();
    for (auto jack : { inJack, outJack })
    {
        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.fillEllipse(jack.x - 9.0f, jack.y - 5.0f, 18.0f, 12.0f);

        juce::ColourGradient jackGradient(juce::Colour(0xFFC4C9CB),
                                          jack.x - 7.0f, jack.y - 7.0f,
                                          juce::Colour(0xFF3E4447),
                                          jack.x + 7.0f, jack.y + 7.0f,
                                          false);
        g.setGradientFill(jackGradient);
        g.fillEllipse(jack.x - 7.0f, jack.y - 7.0f, 14.0f, 14.0f);
        g.setColour(juce::Colours::black);
        g.fillEllipse(jack.x - 3.2f, jack.y - 3.2f, 6.4f, 6.4f);
    }

    auto led = body.reduced(18.0f, 24.0f).withTrimmedBottom(body.getHeight() * 0.70f)
                   .removeFromRight(20.0f);
    const bool active = m_currentType != DspModuleType::BYPASS;
    g.setColour(active ? juce::Colour(0xFF50F07E).withAlpha(0.22f)
                       : juce::Colours::black.withAlpha(0.26f));
    g.fillEllipse(led.expanded(active ? 5.0f : 1.0f));
    g.setColour(active ? juce::Colour(0xFF50F07E) : juce::Colour(0xFF344039));
    g.fillEllipse(led);
    g.setColour(juce::Colours::white.withAlpha(active ? 0.42f : 0.12f));
    g.fillEllipse(led.withSizeKeepingCentre(led.getWidth() * 0.35f, led.getHeight() * 0.22f)
                      .translated(-2.0f, -2.0f));

    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    for (int i = 0; i < 4; ++i)
    {
        auto kb = m_knobs[i].getBounds();
        g.setColour(juce::Colours::black.withAlpha(0.46f));
        g.drawText(knobLabel(m_currentType, i),
                   kb.getX(), kb.getY() - 14, kb.getWidth(), 12,
                   juce::Justification::centred);
        g.setColour(juce::Colours::white.withAlpha(0.72f));
        g.drawText(knobLabel(m_currentType, i),
                   kb.getX(), kb.getY() - 15, kb.getWidth(), 12,
                   juce::Justification::centred);
    }
}

void PedalComponent::resized()
{
    auto bounds = getLocalBounds().reduced(16, 34);
    bounds.removeFromTop(32);
    bounds.removeFromBottom(static_cast<int>(getHeight() * 0.30f));

    auto knobArea = bounds.reduced(2, 0);
    const int halfW = knobArea.getWidth() / 2;
    const int halfH = knobArea.getHeight() / 2;

    for (int i = 0; i < 4; ++i)
    {
        const int row = i / 2;
        const int col = i % 2;
        auto kb = juce::Rectangle<int>(knobArea.getX() + col * halfW,
                                       knobArea.getY() + row * halfH,
                                       halfW,
                                       halfH).reduced(6, 8);
        m_knobs[i].setBounds(kb);
    }
}

void PedalComponent::mouseDown(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds().toFloat().reduced(5.0f, 9.0f);
    bounds.removeFromTop(6.0f);
    auto labelArea = bounds.withTrimmedTop(bounds.getHeight() * 0.67f).reduced(18.0f, 10.0f);

    if (labelArea.contains(event.position))
        showTypePopup();
}

void PedalComponent::mouseMove(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds().toFloat().reduced(5.0f, 9.0f);
    bounds.removeFromTop(6.0f);
    auto labelArea = bounds.withTrimmedTop(bounds.getHeight() * 0.67f).reduced(18.0f, 10.0f);
    setMouseCursor(labelArea.contains(event.position)
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

void PedalComponent::showTypePopup()
{
    juce::PopupMenu menu;
    for (int t = 0; t <= static_cast<int>(DspModuleType::GRANULAR_DELAY); ++t)
    {
        auto type = static_cast<DspModuleType>(t);
        menu.addItem(t + 1, typeName(type), true, type == m_currentType);
    }

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this](int result)
        {
            if (result > 0)
            {
                auto type = static_cast<DspModuleType>(result - 1);
                m_currentType = type;
                repaint();
                audioProcessor.setPedalSlot(m_slotIndex, type);
            }
        });
}

void PedalComponent::syncFromProcessor()
{
    auto type = audioProcessor.getPedalSlot(m_slotIndex);
    if (type != m_currentType)
    {
        m_currentType = type;
        repaint();
    }
}

void PedalComponent::setKnobValue(int knobIdx, float value)
{
    if (knobIdx >= 0 && knobIdx < 4)
        m_knobs[knobIdx].setValue(value, juce::dontSendNotification);
}

juce::Point<float> PedalComponent::getInputJackPos() const
{
    auto bounds = getBounds().toFloat();
    return { bounds.getX() + 42.0f, bounds.getY() + 15.0f };
}

juce::Point<float> PedalComponent::getOutputJackPos() const
{
    auto bounds = getBounds().toFloat();
    return { bounds.getRight() - 42.0f, bounds.getY() + 15.0f };
}
