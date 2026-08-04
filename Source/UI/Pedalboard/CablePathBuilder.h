#pragma once
#include <JuceHeader.h>

struct CachedSplitCable
{
    juce::Path left;
    juce::Path right;
};

namespace CablePathBuilder
{

CachedSplitCable splitCubicBezier(juce::Point<float> p0, juce::Point<float> p1,
                                  juce::Point<float> p2, juce::Point<float> p3);

std::pair<juce::Point<float>, juce::Point<float>> makeSameRowControlPoints(
    juce::Point<float> from, juce::Point<float> to);

juce::Path buildInputCable(juce::Point<float> entryPos, juce::Point<float> jackPos);

juce::Path buildOutputCable(juce::Point<float> jackPos, juce::Point<float> exitPos);

}
