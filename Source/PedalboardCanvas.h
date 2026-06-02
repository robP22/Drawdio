#pragma once
#include <JuceHeader.h>

class DrawdioProcessor;

class PedalboardCanvas : public juce::Component
{
public:
    PedalboardCanvas(DrawdioProcessor& processor);
    ~PedalboardCanvas() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildPedalboardImage();

    DrawdioProcessor& audioProcessor;
    juce::Image m_pedalboardImage;
};
