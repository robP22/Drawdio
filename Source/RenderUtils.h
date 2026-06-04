#pragma once

#include <JuceHeader.h>
#include "ResourceManager.h"

namespace RenderUtils
{
    void drawImageScaled(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds);
    void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness);
}
