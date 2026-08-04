#pragma once
#include <JuceHeader.h>

namespace EditorLayout
{

constexpr float PedalboardWidthRatio = 0.55f;

inline float topOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() == 255)
                return static_cast<float>(y) / static_cast<float>(img.getHeight());
    return 0.0f;
}

inline float bottomOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    for (int y = img.getHeight() - 1; y >= 0; --y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() == 255)
                return static_cast<float>(img.getHeight() - 1 - y) / static_cast<float>(img.getHeight());
    return 0.0f;
}

}
