#pragma once
#include <JuceHeader.h>
#include <cstdint>

class DrawdioProcessor;

class PedalboardCanvas : public juce::Component
{
public:
    PedalboardCanvas(DrawdioProcessor& processor);
    ~PedalboardCanvas() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildFeltImage();

    DrawdioProcessor& audioProcessor;

    juce::Image m_feltImage;
    juce::Rectangle<int> m_boardBounds;
    juce::Rectangle<int> m_feltBounds;
};
