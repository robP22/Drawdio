#pragma once

#include <JuceHeader.h>

#include "PedalComponent.h"

class DrawdioProcessor;

class PedalboardBackground : public juce::Component
{
public:
    PedalboardBackground(DrawdioProcessor& processor);
    ~PedalboardBackground() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void syncFromProcessor();

    PedalComponent* getPedalComponent(int slot) const;

private:
    void loadTexture();

    DrawdioProcessor& audioProcessor;
    juce::Image m_backgroundImage;
    std::array<std::unique_ptr<PedalComponent>, 6> m_pedals;
};
