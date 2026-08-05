#pragma once
#include <JuceHeader.h>

namespace EditorLayout
{

constexpr float PedalboardWidthRatio = 0.55f;

namespace
{

inline bool rowHasOpaque(const juce::Image::BitmapData& data, int y)
{
    const auto* line = data.getLinePointer(y);
    switch (data.pixelFormat)
    {
        case juce::Image::ARGB:
        {
            const auto* px = line;
            for (int x = 0; x < data.width; ++x, px += 4)
                if (px[3] == 255) return true;
            return false;
        }
        case juce::Image::RGB:
            return true;
        default:
        {
            const auto* px = line;
            for (int x = 0; x < data.width; ++x, px += data.pixelStride)
                if (px[data.pixelStride - 1] == 255) return true;
            return false;
        }
    }
}

}

inline float topOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    juce::Image::BitmapData data(img, juce::Image::BitmapData::readOnly);
    const int h = data.height;
    for (int y = 0; y < h; ++y)
        if (rowHasOpaque(data, y))
            return static_cast<float>(y) / static_cast<float>(h);
    return 0.0f;
}

inline float bottomOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    juce::Image::BitmapData data(img, juce::Image::BitmapData::readOnly);
    const int h = data.height;
    for (int y = h - 1; y >= 0; --y)
        if (rowHasOpaque(data, y))
            return static_cast<float>(h - 1 - y) / static_cast<float>(h);
    return 0.0f;
}

}
