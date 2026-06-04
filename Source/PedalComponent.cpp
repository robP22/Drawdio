#include "PedalComponent.h"
#include "PluginProcessor.h"
#include "RenderUtils.h"

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
                               DspModuleType initialType,
                               const ResourceManager& resources,
                               const ThemeManager& theme,
                               PedalSkinManager::PedalSkin skin)
    : audioProcessor(processor),
      m_resources(resources),
      m_theme(theme),
      m_slotIndex(slotIndex),
      m_currentType(initialType),
      m_skin(skin)
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

void PedalComponent::paint(juce::Graphics&)
{
    // EMPTY - sprites only, added later
}

void PedalComponent::setSkin(PedalSkinManager::PedalSkin skin)
{
    if (m_skin != skin)
    {
        m_skin = skin;
        repaint();
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
