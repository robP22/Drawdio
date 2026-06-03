#pragma once

#include <JuceHeader.h>

#include "ResourceManager.h"

namespace RenderUtils
{
    void fillVerticalGloss(juce::Graphics& g,
                           juce::Rectangle<float> bounds,
                           juce::Colour top,
                           juce::Colour bottom,
                           float radius);

    void drawInsetPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float radius);
    void drawSoftShadow(juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float alpha = 0.42f);
    void drawOutline(juce::Graphics& g, juce::Rectangle<float> bounds, float radius, juce::Colour colour, float thickness);
    void drawHighlight(juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float alpha = 0.12f);

    void drawImageScaled(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds);
    void drawTextureClippedToRoundedRect(juce::Graphics& g,
                                         const juce::Image& image,
                                         juce::Rectangle<float> bounds,
                                         float radius,
                                         float opacity);

    void drawSprite(juce::Graphics& g,
                    const ResourceManager& resources,
                    ResourceManager::SpriteId spriteId,
                    juce::Rectangle<float> destination,
                    float opacity = 1.0f);

    void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness);
}
