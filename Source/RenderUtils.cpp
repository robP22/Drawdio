#include "RenderUtils.h"

namespace RenderUtils
{
void drawImageScaled(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds)
{
    if (image.isValid() && !bounds.isEmpty())
        g.drawImage(image,
                    bounds.getX(), bounds.getY(),
                    bounds.getWidth(), bounds.getHeight(),
                    0, 0,
                    image.getWidth(), image.getHeight(),
                    false);
}

void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(thickness,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void paintSurfaceDepth(juce::Graphics& g, juce::Rectangle<float> r)
{
    // Ambient shadow — more passes at lower opacity for smoother falloff
    for (int i = 4; i >= 1; --i)
    {
        g.setColour(juce::Colours::black.withAlpha(0.03f));
        g.fillRoundedRectangle(r.expanded(static_cast<float>(i) * 1.5f), 3.0f);
    }

    // Contact occlusion — 1px dark ring at the perimeter
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.drawRoundedRectangle(r, 1.0f, 1.0f);

    // Edge highlight — 1px light ring just inside the perimeter
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(r.reduced(1.0f), 1.0f, 1.0f);
}
}
