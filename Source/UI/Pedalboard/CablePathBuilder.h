#pragma once
#include <JuceHeader.h>
#include <vector>

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

CachedSplitCable buildWaypointCable(
    const std::vector<juce::Point<float>>& waypoints,
    juce::Point<float> startTangent,
    juce::Point<float> endTangent);

juce::Path buildWaypointPath(
    const std::vector<juce::Point<float>>& waypoints,
    juce::Point<float> startTangent,
    juce::Point<float> endTangent);

}
