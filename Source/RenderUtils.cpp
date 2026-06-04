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
}
