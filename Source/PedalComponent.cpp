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
                                             static_cast<float>(height)).reduced(4.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = rotaryStartAngle
            + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(bounds.translated(0.0f, 2.0f));

        // Outer ring - dark metallic base
        juce::ColourGradient outerGrad(juce::Colour(0xFF3C4144),
                                      centre.x - radius, centre.y - radius,
                                      juce::Colour(0xFF1A1C1E),
                                      centre.x + radius, centre.y + radius,
                                      false);
        g.setGradientFill(outerGrad);
        g.fillEllipse(bounds);

        // Grip grooves on the skirt
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        for (int i = 0; i < 12; ++i)
        {
            float grooveRadius = radius * (0.72f + i * 0.022f);
            g.drawEllipse(
                centre.x - grooveRadius,
                centre.y - grooveRadius,
                grooveRadius * 2.0f,
                grooveRadius * 2.0f,
                0.5f);
        }

        // Inner cap - lighter aluminum top
        auto cap = bounds.reduced(radius * 0.35f);
        juce::ColourGradient capGrad(juce::Colour(0xFFE8ECEE),
                                    cap.getX(), cap.getY(),
                                    juce::Colour(0xFF5B6265),
                                    cap.getRight(), cap.getBottom(),
                                    false);
        capGrad.addColour(0.3f, juce::Colour(0xFFC0C6CA));
        g.setGradientFill(capGrad);
        g.fillEllipse(cap);

        // Specular highlight on cap
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.fillEllipse(juce::Rectangle<float>(
            cap.getX() + cap.getWidth() * 0.2f,
            cap.getY() + cap.getHeight() * 0.15f,
            cap.getWidth() * 0.35f,
            cap.getHeight() * 0.3f));

        // Indicator line
        juce::Path indicator;
        const auto lineStart = centre.getPointOnCircumference(radius * 0.52f, angle);
        const auto lineEnd = centre.getPointOnCircumference(radius * 0.78f, angle);
        indicator.startNewSubPath(lineStart);
        indicator.lineTo(lineEnd);

        g.setColour(juce::Colour(0xFF101214));
        g.strokePath(indicator, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // Outer ring edge highlight
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawEllipse(bounds.reduced(1.0f), 1.0f);
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

    loadSkinTexture();
}

void PedalComponent::loadSkinTexture()
{
    auto texturePath = juce::File::getCurrentWorkingDirectory()
                           .getChildFile("Assets/Skins/pedal_skin_sheet.png");

    if (texturePath.existsAsFile())
    {
        m_skinImage = juce::ImageCache::getFromFile(texturePath);
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

    // Render order: 1. Shadow, 2. Chassis, 3. Surface texture, 4. Labels, 
    //               5. LCD cavity, 6. LCD lens, 7. LCD text, 8. Knobs, 9. LED

    // 1. Shadow
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(body.translated(3.0f, 6.0f), 13.0f);

    // 2. Chassis - rounded metal enclosure with visible side walls
    juce::ColourGradient sideGradient(skinColourForSlot(m_slotIndex).darker(0.65f),
                                      body.getX(), body.getY(),
                                      juce::Colour(0xFF0F1113),
                                      body.getX(), body.getBottom(),
                                      false);
    g.setGradientFill(sideGradient);
    g.fillRoundedRectangle(body, 13.0f);

    // 2b. Side wall depth - left edge
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRect(body.getX(), body.getY() + 10.0f, 4.0f, body.getHeight() - 20.0f);

    // 3. Surface texture (paintable skin layer)
    auto face = body.reduced(7.0f, 6.0f);

    // Use loaded skin image if available
    if (m_skinImage.isValid())
    {
        // Extract the skin for this slot from the skin sheet
        // Skin sheet is arranged horizontally: 6 pedals
        const float skinWidth = m_skinImage.getWidth() / 6.0f;
        const float skinHeight = m_skinImage.getHeight();
        juce::Rectangle<int> skinRegion(
            static_cast<int>(m_slotIndex * skinWidth), 0,
            static_cast<int>(skinWidth), static_cast<int>(skinHeight));

        // Extract and resize skin to fit the face area
        auto skin = m_skinImage.getClippedImage(skinRegion);
        if (skin.isValid())
        {
            auto resizedSkin = skin.rescaled(
                juce::jlimit(1, 1000, static_cast<int>(face.getWidth())),
                juce::jlimit(1, 1000, static_cast<int>(face.getHeight())),
                juce::Graphics::highResamplingQuality);
            g.drawImage(resizedSkin, face);
        }
        else
        {
            // Fallback to procedural
            juce::Colour skinColor = skinColourForSlot(m_slotIndex);
            juce::ColourGradient faceGradient(skinColor.brighter(0.12f), face.getX(), face.getY(),
                                              skinColor.darker(0.55f), face.getX(), face.getBottom(),
                                              false);
            g.setGradientFill(faceGradient);
            g.fillRoundedRectangle(face, 10.0f);
        }
    }
    else
    {
        // Procedural fallback
        juce::Colour skin = skinColourForSlot(m_slotIndex);
        juce::ColourGradient faceGradient(skin.brighter(0.12f), face.getX(), face.getY(),
                                          skin.darker(0.55f), face.getX(), face.getBottom(),
                                          false);
        g.setGradientFill(faceGradient);
        g.fillRoundedRectangle(face, 10.0f);
    }

    // Texture noise for realistic paint (only with procedural skin)
    if (!m_skinImage.isValid())
    {
        juce::Random random(0xD0A0 + m_slotIndex);
        for (int i = 0; i < 120; ++i)
        {
            const auto x = face.getX() + random.nextFloat() * face.getWidth();
            const auto y = face.getY() + random.nextFloat() * face.getHeight();
            g.setColour(juce::Colours::white.withAlpha(random.nextFloat() * 0.04f));
            g.fillRect(x, y, 1.0f + random.nextFloat() * 2.5f, 0.7f + random.nextFloat());
        }
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

    // Edge highlight for 3D effect
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(face.reduced(1.0f), 9.0f, 1.0f);

    // Bottom edge shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRoundedRectangle(body, 13.0f, 1.8f);

    // Knob labels with contact shadow
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    for (int i = 0; i < 4; ++i)
    {
        auto kb = m_knobs[i].getBounds();
        g.setColour(juce::Colours::black.withAlpha(0.48f));
        g.drawText(knobLabel(m_currentType, i),
                   kb.getX(), kb.getY() - 13, kb.getWidth(), 12,
                   juce::Justification::centred);
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.drawText(knobLabel(m_currentType, i),
                   kb.getX(), kb.getY() - 14, kb.getWidth(), 12,
                   juce::Justification::centred);
    }
}

void PedalComponent::resized()
{
    auto bounds = getLocalBounds().reduced(16, 34);
    bounds.removeFromTop(32);
    bounds.removeFromBottom(static_cast<int>(getHeight() * 0.27f));

    auto knobArea = bounds.reduced(2, 0);
    const int halfW = knobArea.getWidth() / 2;
    const int halfH = knobArea.getHeight() / 2;

    for (int i = 0; i < 4; ++i)
    {
        const int row = i / 2;
        const int col = i % 2;
        // Make knobs square for circular appearance
        const int knobSize = juce::jmin(halfW, halfH) - 6;
        auto kb = juce::Rectangle<int>(knobArea.getX() + col * halfW + (halfW - knobSize) / 2,
                                       knobArea.getY() + row * halfH + (halfH - knobSize) / 2,
                                       knobSize,
                                       knobSize);
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
