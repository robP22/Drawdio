#pragma once
#include <JuceHeader.h>
#include "Core/EditorDesignMetrics.h"

namespace EditorLayout
{

struct LayoutResult
{
    float scale = 0.0f;
    juce::Rectangle<int> content;
    juce::Rectangle<int> topArea;
    juce::Rectangle<int> bottomBar;
    juce::Rectangle<int> pedalboardArea;
    juce::Rectangle<int> canvasArea;
    juce::Rectangle<int> header;
    juce::Rectangle<int> palette;
    juce::Rectangle<int> pixelCanvas;
};

inline LayoutResult calculate(juce::Rectangle<int> bounds)
{
    LayoutResult result;

    if (bounds.isEmpty())
        return result;

    const float scale = std::min(
        static_cast<float>(bounds.getWidth()) / static_cast<float>(EditorDesignMetrics::DesignResolution::Width),
        static_cast<float>(bounds.getHeight()) / static_cast<float>(EditorDesignMetrics::DesignResolution::Height));
    const int contentW = std::min(bounds.getWidth(), juce::roundToInt(
        static_cast<float>(EditorDesignMetrics::DesignResolution::Width) * scale));
    const int contentH = std::min(bounds.getHeight(), juce::roundToInt(
        static_cast<float>(EditorDesignMetrics::DesignResolution::Height) * scale));
    result.content = bounds.withSizeKeepingCentre(contentW, contentH);
    result.scale = static_cast<float>(contentW)
                 / static_cast<float>(EditorDesignMetrics::DesignResolution::Width);

    auto remaining = result.content;
    const int bottomBarH = juce::roundToInt(remaining.getHeight() * EditorDesignMetrics::BottomBar::HeightRatio);
    result.topArea = remaining.removeFromTop(remaining.getHeight() - bottomBarH);
    result.bottomBar = remaining;

    const int pedalW = juce::roundToInt(result.topArea.getWidth() * EditorDesignMetrics::PedalboardWidthRatio);
    const int canvasW = result.topArea.getWidth() - pedalW;
    result.pedalboardArea = result.topArea.withTrimmedLeft(canvasW);
    result.canvasArea = result.topArea.withTrimmedRight(pedalW);

    const int headerH = juce::roundToInt(result.pedalboardArea.getHeight() * EditorDesignMetrics::PedalboardHeader::HeightRatio);
    result.header = result.pedalboardArea.withTrimmedBottom(result.pedalboardArea.getHeight() - headerH);

    const int paletteH = juce::roundToInt(result.canvasArea.getHeight() * EditorDesignMetrics::PaletteHeightRatio);
    const int paletteInset = juce::roundToInt(result.canvasArea.getWidth() * EditorDesignMetrics::PaletteColumnInsetRatio);
    result.palette = result.canvasArea
                         .withTrimmedTop(result.canvasArea.getHeight() - paletteH)
                         .withTrimmedLeft(paletteInset)
                         .withTrimmedRight(paletteInset);

    const auto canvasRegion = result.canvasArea.withTrimmedBottom(paletteH);
    const int squareSize = canvasRegion.getHeight();
    result.pixelCanvas = canvasRegion.withSizeKeepingCentre(squareSize, squareSize);
    return result;
}

inline float scaleFromHeight(float componentHeight, float designRatio)
{
    return componentHeight / (designRatio * static_cast<float>(EditorDesignMetrics::DesignResolution::Height));
}

inline float scaledCap(float usable, float ratio, float designPx, float scale)
{
    return std::min(usable * ratio, designPx * scale);
}

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
