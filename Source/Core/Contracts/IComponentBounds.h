#pragma once
#include <JuceHeader.h>

class IComponentBounds
{
public:
    virtual ~IComponentBounds() = default;
    virtual juce::Rectangle<int> getBounds() const = 0;
    virtual void setBounds(juce::Rectangle<int>) = 0;
    virtual juce::Point<float> getInputJackPos() const = 0;
    virtual juce::Point<float> getOutputJackPos() const = 0;
};
