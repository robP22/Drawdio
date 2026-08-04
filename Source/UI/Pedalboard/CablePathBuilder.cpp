#include "CablePathBuilder.h"

CachedSplitCable CablePathBuilder::splitCubicBezier(
    juce::Point<float> p0, juce::Point<float> p1,
    juce::Point<float> p2, juce::Point<float> p3)
{
    const auto p01  = (p0 + p1) * 0.5f;
    const auto p12  = (p1 + p2) * 0.5f;
    const auto p23  = (p2 + p3) * 0.5f;
    const auto p012 = (p01 + p12) * 0.5f;
    const auto p123 = (p12 + p23) * 0.5f;
    const auto p0123 = (p012 + p123) * 0.5f;

    CachedSplitCable result;
    result.left.startNewSubPath(p0);
    result.left.cubicTo(p01, p012, p0123);
    result.right.startNewSubPath(p0123);
    result.right.cubicTo(p123, p23, p3);
    return result;
}

std::pair<juce::Point<float>, juce::Point<float>> CablePathBuilder::makeSameRowControlPoints(
    juce::Point<float> from, juce::Point<float> to)
{
    const float h = std::abs(to.x - from.x);
    const float lift = std::min(h * 0.06f, 10.0f);
    const float curve = std::max(h * 0.28f, 20.0f);
    const float clampedCurve = std::min(curve, h * 0.4f);
    const float dir = (from.x < to.x) ? 1.0f : -1.0f;
    return {
        juce::Point<float>{from.x + clampedCurve * dir, from.y - lift},
        juce::Point<float>{to.x - clampedCurve * dir, to.y - lift}
    };
}

juce::Path CablePathBuilder::buildInputCable(juce::Point<float> entryPos, juce::Point<float> jackPos)
{
    juce::Path path;
    const float vertDist = jackPos.y - entryPos.y;
    const float lift = vertDist * 0.4f;
    const float offsetX = 30.0f;

    path.startNewSubPath(entryPos);
    path.cubicTo(entryPos.x + offsetX, entryPos.y + lift,
                 jackPos.x + offsetX, jackPos.y - lift,
                 jackPos.x, jackPos.y);
    return path;
}

juce::Path CablePathBuilder::buildOutputCable(juce::Point<float> jackPos, juce::Point<float> exitPos)
{
    juce::Path path;
    const float vertDist = jackPos.y - exitPos.y;
    const float lift = vertDist * 0.4f;
    const float offsetX = 30.0f;

    path.startNewSubPath(jackPos);
    path.cubicTo(jackPos.x - offsetX, jackPos.y - lift,
                 exitPos.x - offsetX, exitPos.y + lift,
                 exitPos.x, exitPos.y);
    return path;
}
