#include "PedalComponent.h"
#include "PluginProcessor.h"

// -----------------------------------------------------------------------
// Colour constants
// -----------------------------------------------------------------------
static const auto kChassisColour   = juce::Colour(0xFF1A1A1A);
static const auto kBezelColour     = juce::Colour(0xFF2D2D2D);
static const auto kLcdBgColour     = juce::Colour(0xFF0F0F0F);

// -----------------------------------------------------------------------
// Pedal type display names
// -----------------------------------------------------------------------
const char* PedalComponent::typeName(DspModuleType t)
{
    switch (t)
    {
        case DspModuleType::BYPASS:                   return "Bypass";
        case DspModuleType::WAVESHAPER_DISTORTION:     return "Waveshaper Dist.";
        case DspModuleType::MODULATED_DELAY_LINE:      return "Mod. Delay";
        case DspModuleType::BIQUAD_FILTER:             return "Biquad Filter";
        case DspModuleType::DYNAMIC_RING_BUFFER:        return "Dyn. Ring Buffer";
        case DspModuleType::PITCH_SHIFTER_GRANULAR:    return "Pitch Shifter";
        case DspModuleType::ENVELOPE_VCA_COMPRESSOR:   return "VCA Compressor";
        case DspModuleType::PITCH_DETECTOR_OSCILLATOR: return "Pitch Detector";
        case DspModuleType::DIFFUSED_DELAY_NETWORK:    return "Diff. Delay Net";
        case DspModuleType::ALLPASS_FILTER_CASCADE:    return "Allpass Cascade";
        case DspModuleType::FREQUENCY_SHIFTER:         return "Freq. Shifter";
        case DspModuleType::MATHEMATICAL_WAVEFOLDER:   return "Wavefolder";
        case DspModuleType::SAMPLE_RATE_DEGRADER:      return "Sample Rate Deg.";
        case DspModuleType::FORMANT_VOCAL_SHIFTER:     return "Formant Shifter";
        case DspModuleType::TAPE_STOP_REVERSE_ECHO:    return "Tape Stop Echo";
        case DspModuleType::SIMPLE_DELAY:              return "Simple Delay";
        case DspModuleType::PLATE_REVERB:              return "Plate Reverb";
        case DspModuleType::SOFT_DISTORTION:           return "Soft Distortion";
        case DspModuleType::GRANULAR_DELAY:            return "Gran. Delay";
        default: return "Unknown";
    }
}

// -----------------------------------------------------------------------
// Knob label for a given pedal type + knob index (0-3: Wet, Dry, Vol, Effect)
// -----------------------------------------------------------------------
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

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------
PedalComponent::PedalComponent(DrawdioProcessor& processor, int slotIndex, DspModuleType initialType)
    : audioProcessor(processor), m_slotIndex(slotIndex), m_currentType(initialType)
{
    for (int k = 0; k < 4; ++k)
        initKnob(m_knobs[k]);
}

void PedalComponent::initKnob(juce::Slider& knob)
{
    addAndMakeVisible(knob);
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
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

        if (knobIdx != -1)
            audioProcessor.getDSPProcessor().updateParameter(m_slotIndex, knobIdx, static_cast<float>(knob.getValue()));
    };
}

// -----------------------------------------------------------------------
// Paint
// -----------------------------------------------------------------------
void PedalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto chassisBounds = bounds.reduced(4.0f);

    // Matte black chassis
    g.setColour(kChassisColour);
    g.fillRoundedRectangle(chassisBounds, 12.0f);

    // Inner bezel
    g.setColour(kBezelColour);
    g.drawRoundedRectangle(chassisBounds.reduced(2.0f), 10.0f, 2.0f);

    // LCD / label background at bottom
    auto labelArea = chassisBounds.removeFromBottom(chassisBounds.getHeight() * 0.3f).reduced(12.0f, 8.0f);
    g.setColour(kLcdBgColour);
    g.fillRoundedRectangle(labelArea, 4.0f);

    // Pedal type name text in LCD area
    g.setColour(juce::Colours::whitesmoke);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(typeName(m_currentType),
               labelArea.toNearestInt(),
               juce::Justification::centred);

    // Draw Jack Sockets (now more visible outside the chassis)
    auto inJack = getInputJackPos() - getPosition().toFloat();
    auto outJack = getOutputJackPos() - getPosition().toFloat();
    
    // Metallic ring
    g.setColour(juce::Colours::silver);
    g.drawEllipse(inJack.x - 5.0f, inJack.y - 5.0f, 10.0f, 10.0f, 1.0f);
    g.drawEllipse(outJack.x - 5.0f, outJack.y - 5.0f, 10.0f, 10.0f, 1.0f);

    g.setColour(juce::Colours::grey);
    g.fillEllipse(inJack.x - 4.0f, inJack.y - 4.0f, 8.0f, 8.0f);
    g.fillEllipse(outJack.x - 4.0f, outJack.y - 4.0f, 8.0f, 8.0f);
    g.setColour(juce::Colours::black);
    g.fillEllipse(inJack.x - 2.0f, inJack.y - 2.0f, 4.0f, 4.0f);
    g.fillEllipse(outJack.x - 2.0f, outJack.y - 2.0f, 4.0f, 4.0f);

    // Knob labels (drawn above each knob, inside the pedal)
    g.setFont(juce::FontOptions(9.0f));
    for (int i = 0; i < 4; ++i)
    {
        auto kb = m_knobs[i].getBounds();
        g.setColour(juce::Colours::lightgrey.withAlpha(0.7f));
        g.drawText(knobLabel(m_currentType, i),
                   kb.getX(), kb.getY() - 13, kb.getWidth(), 11,
                   juce::Justification::centred);
    }
}

// -----------------------------------------------------------------------
// Layout
// -----------------------------------------------------------------------
void PedalComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(static_cast<int>(bounds.getHeight() * 0.3f));

    // Knobs in 2x2 grid
    auto knobArea = bounds.reduced(6, 4);
    int halfW = knobArea.getWidth() / 2;
    int halfH = knobArea.getHeight() / 2;

    for (int i = 0; i < 4; ++i)
    {
        int row = i / 2;
        int col = i % 2;
        auto kb = juce::Rectangle<int>(
            knobArea.getX() + col * halfW,
            knobArea.getY() + row * halfH,
            halfW,
            halfH).reduced(4, 2);
        m_knobs[i].setBounds(kb);
    }
}

// -----------------------------------------------------------------------
// Mouse interaction
// -----------------------------------------------------------------------
void PedalComponent::mouseDown(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds();
    auto labelArea = bounds.removeFromBottom(static_cast<int>(bounds.getHeight() * 0.3f)).reduced(12, 8);
    if (labelArea.contains(event.getPosition()))
        showTypePopup();
}

void PedalComponent::mouseMove(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds();
    auto labelArea = bounds.removeFromBottom(static_cast<int>(bounds.getHeight() * 0.3f)).reduced(12, 8);
    setMouseCursor(labelArea.contains(event.getPosition())
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

// -----------------------------------------------------------------------
// Pedal type popup
// -----------------------------------------------------------------------
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

// -----------------------------------------------------------------------
// State sync
// -----------------------------------------------------------------------
void PedalComponent::setPedalType(DspModuleType type)
{
    if (type == m_currentType) return;
    m_currentType = type;
    repaint();
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

// -----------------------------------------------------------------------
// Jack positions (in parent / editor coordinates)
// -----------------------------------------------------------------------
juce::Point<float> PedalComponent::getInputJackPos() const
{
    auto bounds = getBounds();
    return juce::Point<float>(
        static_cast<float>(bounds.getRight()),
        static_cast<float>(bounds.getCentreY()));
}

juce::Point<float> PedalComponent::getOutputJackPos() const
{
    auto bounds = getBounds();
    return juce::Point<float>(
        static_cast<float>(bounds.getX()),
        static_cast<float>(bounds.getCentreY()));
}
