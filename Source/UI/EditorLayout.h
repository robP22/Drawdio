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

inline bool columnHasOpaque(const juce::Image::BitmapData& data, int x)
{
    switch (data.pixelFormat)
    {
        case juce::Image::ARGB:
        {
            const auto* px = data.getPixelPointer(x, 0);
            for (int y = 0; y < data.height; ++y, px += data.lineStride)
                if (px[3] == 255) return true;
            return false;
        }
        case juce::Image::RGB:
            return true;
        default:
        {
            const auto* px = data.getPixelPointer(x, 0);
            for (int y = 0; y < data.height; ++y, px += data.lineStride)
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

inline float leftOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    juce::Image::BitmapData data(img, juce::Image::BitmapData::readOnly);
    const int w = data.width;
    for (int x = 0; x < w; ++x)
        if (columnHasOpaque(data, x))
            return static_cast<float>(x) / static_cast<float>(w);
    return 0.0f;
}

inline float rightOpaqueRatio(const juce::Image& img)
{
    if (!img.isValid()) return 0.0f;
    juce::Image::BitmapData data(img, juce::Image::BitmapData::readOnly);
    const int w = data.width;
    for (int x = w - 1; x >= 0; --x)
        if (columnHasOpaque(data, x))
            return static_cast<float>(w - 1 - x) / static_cast<float>(w);
    return 0.0f;
}

}
