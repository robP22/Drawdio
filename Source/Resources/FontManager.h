#pragma once
#include <JuceHeader.h>

// Bundled-font registry. The active face is selected once at initialise() and
// installed as the default sans-serif typeface, so every juce::Font(size) in
// the program renders it. The future user font-picker swaps the face here.
class FontManager
{
public:
    enum class PixelVariant { Circle, Grid, Line, Square, Triangle };

    static void initialise(PixelVariant variant = kDefaultVariant);

    static const juce::Typeface::Ptr& face() { return s_face; }

    static PixelVariant kDefaultVariant;

private:
    static juce::Typeface::Ptr s_face;
};
