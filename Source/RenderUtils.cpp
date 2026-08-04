#include "RenderUtils.h"

namespace RenderUtils
{
void strokeCable(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(thickness,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

void paintEdgeHighlight(juce::Graphics& g, juce::Rectangle<float> r, float c)
{
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(r.reduced(1.0f), c, 1.5f);
}

void paintCurvatureVignette(juce::Graphics& g, juce::Rectangle<float> r, float c)
{
    g.setGradientFill(juce::ColourGradient(
        juce::Colours::transparentWhite, r.getCentre(),
        juce::Colours::black.withAlpha(0.15f), r.getTopLeft(), true));
    g.fillRoundedRectangle(r, c);
}

const juce::Image& getNoiseTexture()
{
    static const juce::Image noise = []()
    {
        juce::Image img(juce::Image::RGB, 128, 128, false);
        juce::Random rng;
        rng.setSeed(42);
        juce::Image::BitmapData data(img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 128; ++x)
            {
                auto v = static_cast<uint8>(rng.nextInt(256));
                auto* p = data.getPixelPointer(x, y);
                p[0] = v; p[1] = v; p[2] = v;
            }
        return img;
    }();
    return noise;
}
}
