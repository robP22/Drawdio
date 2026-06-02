#pragma once

#include <JuceHeader.h>

class DrawdioProcessor;

class PedalboardBackground : public juce::Component
{
public:
    PedalboardBackground(DrawdioProcessor& processor);
    ~PedalboardBackground() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void loadTexture();

    DrawdioProcessor& audioProcessor;
    juce::Image m_backgroundImage;
};
