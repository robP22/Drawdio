#pragma once

#include <JuceHeader.h>

namespace RenderUtils
{
    void drawImageScaled(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds);
    void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness);
    void paintSurfaceDepth(juce::Graphics& g, juce::Rectangle<float> bounds);
    
    /// Apply accent-colored button styling with hover feedback
    /// @param button The TextButton to style
    /// @param accentColour The primary accent color for the button's active/pressed state
    void styleAccentButton(juce::TextButton& button, juce::Colour accentColour);
}
