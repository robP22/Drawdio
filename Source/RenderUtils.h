#pragma once

#include <JuceHeader.h>

namespace RenderUtils
{
    void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness);
    void paintCurvatureVignette(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius);
    void paintEdgeHighlight(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius);
    const juce::Image& getNoiseTexture();
}
