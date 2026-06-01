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
                                             static_cast<float>(height));
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle
            + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse(bounds.reduced(2.0f).translated(0.0f, 3.0f));

        // 10-lobe scalloped outer body
        juce::Path bodyPath;
        const int lobes = 10;
        const float outerRadius = radius;
        const float lobeDepth = radius * 0.08f;

        for (int i = 0; i < lobes; ++i)
        {
            float a1 = i * 2.0f * juce::MathConstants<float>::pi / lobes - juce::MathConstants<float>::pi / 2.0f;
            float a2 = (i + 0.5f) * 2.0f * juce::MathConstants<float>::pi / lobes - juce::MathConstants<float>::pi / 2.0f;
            float a3 = (i + 1.0f) * 2.0f * juce::MathConstants<float>::pi / lobes - juce::MathConstants<float>::pi / 2.0f;

            auto p1 = centre.getPointOnCircumference(outerRadius - lobeDepth, a1);
            auto p2 = centre.getPointOnCircumference(outerRadius, a2);
            auto p3 = centre.getPointOnCircumference(outerRadius - lobeDepth, a3);

            if (i == 0)
                bodyPath.startNewSubPath(p1);
            else
                bodyPath.lineTo(p1);
            bodyPath.lineTo(p2);
            bodyPath.lineTo(p3);
        }
        bodyPath.closeSubPath();

        // Body gradient - matte black plastic (#1A1A1A)
        juce::ColourGradient bodyGrad(
            juce::Colour(0xFF2A2A2A), centre.x, centre.y - radius,
            juce::Colour(0xFF0A0A0A), centre.x, centre.y + radius,
            true);
        bodyGrad.addColour(0.5f, juce::Colour(0xFF1A1A1A));
        g.setGradientFill(bodyGrad);
        g.fillPath(bodyPath);

        // Inner shadow ring (recessed edge around cap)
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(juce::Rectangle<float>(
            centre.x - radius * 0.82f, centre.y - radius * 0.82f,
            radius * 1.64f, radius * 1.64f));

        // Brushed aluminum cap with concentric lathe rings
        auto capBounds = juce::Rectangle<float>(
            centre.x - radius * 0.78f, centre.y - radius * 0.78f,
            radius * 1.56f, radius * 1.56f);

        // Base aluminum gradient
        juce::ColourGradient capGrad(
            juce::Colour(0xFFE8ECEE), centre.x, centre.y - radius * 0.78f,
            juce::Colour(0xFF5B6265), centre.x, centre.y + radius * 0.78f,
            true);
        capGrad.addColour(0.3f, juce::Colour(0xFFC0C6CA));
        capGrad.addColour(0.7f, juce::Colour(0xFF6B7175));
        g.setGradientFill(capGrad);
        g.fillEllipse(capBounds);

        // Concentric lathe rings (brushed texture)
        g.setColour(juce::Colour(0x20000000));
        for (int r = 1; r <= 6; ++r)
        {
            float ringRadius = radius * 0.78f * (r / 6.0f);
            g.drawEllipse(
                centre.x - ringRadius, centre.y - ringRadius,
                ringRadius * 2.0f, ringRadius * 2.0f,
                0.5f);
        }

        // Indicator line at 12 o'clock (rotates with knob)
        juce::Path indicator;
        const auto lineStart = centre.getPointOnCircumference(radius * 0.78f, angle - juce::MathConstants<float>::pi / 2.0f);
        const auto lineEnd = centre.getPointOnCircumference(radius * 0.55f, angle - juce::MathConstants<float>::pi / 2.0f);
        indicator.startNewSubPath(lineStart);
        indicator.lineTo(lineEnd);

        g.setColour(juce::Colours::white);
        g.strokePath(indicator, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // Specular highlight - top-left quadrant
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.fillEllipse(juce::Rectangle<float>(
            centre.x - radius * 0.4f,
            centre.y - radius * 0.6f,
            radius * 0.35f,
            radius * 0.22f));

        // Body edge highlight
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawEllipse(bounds.reduced(1.5f), 1.0f);
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
    
    for (auto& label : m_knobLabels)
    {
        addAndMakeVisible(label);
        label.setFont(juce::Font(14.0f));
        label.setColour(juce::Label::textColourId, juce::Colour(0xFF2A2A2A));
        label.setJustificationType(juce::Justification::centred);
    }

    updateKnobLabels();
    loadSkinTexture();
}

void PedalComponent::loadSkinTexture()
{
    auto texturePath = juce::File::getSpecialLocation(juce::File::invokedExecutableFile)
                           .getParentDirectory()
                           .getChildFile("Contents/Resources/Assets/Skins/Pedal_Texture_Crayon.png");

    if (!texturePath.existsAsFile())
    {
        texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Skins/Pedal_Texture_Crayon.png");
    }

    if (texturePath.existsAsFile())
    {
        m_skinImage = juce::ImageCache::getFromFile(texturePath);
    }
}

void PedalComponent::updateKnobLabels()
{
    for (int i = 0; i < 4; ++i)
    {
        m_knobLabels[i].setText(knobLabel(m_currentType, i), juce::dontSendNotification);
    }
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

    // Render order: 1. Shadow, 2. Texture, 3. LCD, 4. Jacks, 5. LED 
    //               

    // 1. Shadow
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(body.translated(3.0f, 6.0f), 13.0f);

    // 2. Surface texture - draw full pedal image
    if (m_skinImage.isValid())
    {
        g.drawImage(m_skinImage, body);
    }

    // 5. LCD cavity - appears raised with bevel effect
    auto lcdArea = body.withTrimmedTop(body.getHeight() * 0.62f).reduced(18.0f, 10.0f);

    // LCD raised outer frame (bevel)
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(lcdArea.expanded(4.0f), 6.0f);

    // LCD raised highlight (top-left bevel)
    g.setColour(juce::Colour(0xFF888888));
    g.drawLine(lcdArea.getX() + 4.0f, lcdArea.getY() + 1.0f,
               lcdArea.getRight() - 4.0f, lcdArea.getY() + 1.0f, 2.0f);
    g.drawLine(lcdArea.getX() + 1.0f, lcdArea.getY() + 4.0f,
               lcdArea.getX() + 1.0f, lcdArea.getBottom() - 4.0f, 2.0f);

    // LCD dark cavity
    g.setColour(juce::Colour(0xFF080A09));
    g.fillRoundedRectangle(lcdArea, 5.0f);

    // 6. LCD lens - recessed glass
    juce::ColourGradient lens(juce::Colour(0xFF182018),
                              lcdArea.getX(), lcdArea.getY(),
                              juce::Colour(0xFF020402),
                              lcdArea.getRight(), lcdArea.getBottom(),
                              false);
    g.setGradientFill(lens);
    g.fillRoundedRectangle(lcdArea.reduced(3.0f), 4.0f);

    // 7. LCD text - fluorescent green
    g.setColour(juce::Colour(0xFFA8F0B8));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawFittedText(typeName(m_currentType),
                     lcdArea.reduced(6.0f, 3.0f).toNearestInt(),
                     juce::Justification::centred,
                     1);

    // Draw input/output jacks with proper depth
    auto inJack = getInputJackPos() - getPosition().toFloat();
    auto outJack = getOutputJackPos() - getPosition().toFloat();

    for (auto jack : { inJack, outJack })
    {
        // Jack socket shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse(jack.x - 10.0f, jack.y - 6.0f, 20.0f, 14.0f);

        // Jack ring
        juce::ColourGradient jackGrad(juce::Colour(0xFFD4D9DB),
                                      jack.x - 8.0f, jack.y - 8.0f,
                                      juce::Colour(0xFF4E5457),
                                      jack.x + 8.0f, jack.y + 8.0f,
                                      false);
        g.setGradientFill(jackGrad);
        g.fillEllipse(jack.x - 8.0f, jack.y - 8.0f, 16.0f, 16.0f);

        // Jack hole
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.fillEllipse(jack.x - 4.0f, jack.y - 4.0f, 8.0f, 8.0f);

        // Jack inner highlight
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawEllipse(jack.x - 6.0f, jack.y - 6.0f, 12.0f, 12.0f, 0.5f);
    }

    // 9. LED with glow - centered horizontally, below top edge
    auto led = juce::Rectangle<float>(body.getX() + body.getWidth() * 0.5f - 5.0f,
                                      body.getY() + 14.0f,
                                      10.0f, 10.0f);
    const bool active = m_currentType != DspModuleType::BYPASS;

    // LED glow
    if (active)
    {
        g.setColour(juce::Colour(0xFF50F07E).withAlpha(0.25f));
        g.fillEllipse(led.expanded(6.0f));
        g.setColour(juce::Colour(0xFF50F07E).withAlpha(0.15f));
        g.fillEllipse(led.expanded(10.0f));
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillEllipse(led.expanded(2.0f));
    }

    // LED body
    g.setColour(active ? juce::Colour(0xFF50F07E) : juce::Colour(0xFF304030));
    g.fillEllipse(led);

    // LED specular highlight
    g.setColour(juce::Colours::white.withAlpha(active ? 0.5f : 0.1f));
    g.fillEllipse(led.withSizeKeepingCentre(led.getWidth() * 0.35f, led.getHeight() * 0.22f)
                      .translated(-2.0f, -2.0f));

    // Bottom edge shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(body, 13.0f, 1.8f);

}

void PedalComponent::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();

    // Knob positions relative to component size
    // Simulating positions: (365,430), (758,430), (365,755), (758,755) relative to 900x600 base
    const int knobSize = juce::jmin(w, h) / 5;
    const int labelHeight = 20;

    // Position knobs based on component bounds (centered in each quadrant)
    m_knobs[0].setBounds(w * 26 / 100 - knobSize / 2, h * 33 / 100 - knobSize / 2, knobSize, knobSize);
    m_knobs[1].setBounds(w * 54 / 100 - knobSize / 2, h * 33 / 100 - knobSize / 2, knobSize, knobSize);
    m_knobs[2].setBounds(w * 26 / 100 - knobSize / 2, h * 58 / 100 - knobSize / 2, knobSize, knobSize);
    m_knobs[3].setBounds(w * 54 / 100 - knobSize / 2, h * 58 / 100 - knobSize / 2, knobSize, knobSize);

    // Position labels 10px below each knob
    for (int i = 0; i < 4; ++i)
    {
        auto knobBounds = m_knobs[i].getBounds();
        m_knobLabels[i].setBounds(knobBounds.getX(), knobBounds.getBottom() + 10, knobSize, labelHeight);
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
                updateKnobLabels();
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
        updateKnobLabels();
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
