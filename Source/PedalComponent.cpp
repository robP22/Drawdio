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

namespace
{
