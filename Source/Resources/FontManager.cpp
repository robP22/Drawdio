#include "FontManager.h"

juce::Typeface::Ptr FontManager::s_face;
FontManager::PixelVariant FontManager::kDefaultVariant = FontManager::PixelVariant::Square;

void FontManager::initialise(PixelVariant variant)
{
    if (s_face != nullptr)
        return;

    const char* data = nullptr;
    int size = 0;
    switch (variant)
    {
        case PixelVariant::Circle:   data = BinaryData::GeistPixelCircle_ttf;   size = BinaryData::GeistPixelCircle_ttfSize;   break;
        case PixelVariant::Grid:     data = BinaryData::GeistPixelGrid_ttf;     size = BinaryData::GeistPixelGrid_ttfSize;     break;
        case PixelVariant::Line:     data = BinaryData::GeistPixelLine_ttf;     size = BinaryData::GeistPixelLine_ttfSize;     break;
        case PixelVariant::Triangle: data = BinaryData::GeistPixelTriangle_ttf; size = BinaryData::GeistPixelTriangle_ttfSize; break;
        case PixelVariant::Square:
        default:                     data = BinaryData::GeistPixelSquare_ttf;  size = BinaryData::GeistPixelSquare_ttfSize;  break;
    }

    s_face = juce::Typeface::createSystemTypefaceFor(data, size);
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface(s_face);
}
