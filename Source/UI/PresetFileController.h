#pragma once
#include <JuceHeader.h>
#include <functional>

class PresetFileController
{
public:
    static void savePreset(std::function<juce::MemoryBlock()> createState);
    static void loadPreset(std::function<bool(const void*, int)> applyState,
                           std::function<void()> onLoaded);
};
