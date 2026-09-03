#include "CablePathBuilder.h"
#include "Core/EditorDesignMetrics.h"
#include <algorithm>
#include <cmath>
#include <functional>

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
    const float lift = juce::jlimit(EditorDesignMetrics::Cable::JackRiseMinPx,
                                    h * EditorDesignMetrics::Cable::JackRiseSpanRatio,
                                    EditorDesignMetrics::Cable::JackRiseMaxPx);
    const float curve = std::max(h * 0.28f, EditorDesignMetrics::Cable::CurveMinPx);
    const float clampedCurve = std::min(curve, h * 0.4f);
    const float dir = (from.x < to.x) ? 1.0f : -1.0f;
    return {
        juce::Point<float>{from.x + clampedCurve * dir, from.y - lift},
        juce::Point<float>{to.x - clampedCurve * dir, to.y - lift}
    };
}

namespace
{
struct WaypointSegment
{
    juce::Point<float> p0, p1, p2, p3;
};

void appendWaypointSegments(const std::vector<juce::Point<float>>& waypoints,
                            float maxCurve,
                            const std::function<void(const WaypointSegment&)>& sink)
{
    if (waypoints.size() < 2)
        return;

    const auto unit = [](juce::Point<float> v)
    {
        const float len = v.getDistanceFromOrigin();
        if (len < 1.0e-6f)
            return juce::Point<float>(0.0f, -1.0f);
        return v / len;
    };

    std::vector<float> segLens;
    segLens.reserve(waypoints.size() > 0 ? waypoints.size() - 1 : 0);
    for (size_t i = 0; i + 1 < waypoints.size(); ++i)
        segLens.push_back(waypoints[i].getDistanceFrom(waypoints[i + 1]));

    for (size_t i = 0; i + 1 < waypoints.size(); ++i)
    {
        const auto& a = waypoints[i];
        const auto& b = waypoints[i + 1];
        const float segLen = segLens[i];

        juce::Point<float> dirIn;
        if (i == 0)
            dirIn = unit(b - a);
        else
            dirIn = unit(waypoints[i + 1] - waypoints[i - 1]);

        juce::Point<float> dirOut;
        if (i + 2 == waypoints.size())
            dirOut = unit(b - a);
        else
            dirOut = unit(waypoints[i + 2] - waypoints[i]);

        float k1;
        if (i == 0)
            k1 = std::min(segLen * 0.45f, maxCurve);
        else
            k1 = std::min(std::min(segLens[i - 1], segLen) * 0.45f, maxCurve);

        float k2;
        if (i + 2 == waypoints.size())
            k2 = std::min(segLen * 0.45f, maxCurve);
        else
            k2 = std::min(std::min(segLen, segLens[i + 1]) * 0.45f, maxCurve);

        sink({ a, a + dirIn * k1, b - dirOut * k2, b });
    }
}
}

CachedSplitCable CablePathBuilder::buildWaypointCable(
    const std::vector<juce::Point<float>>& waypoints,
    float maxCurve)
{
    CachedSplitCable result;
    appendWaypointSegments(waypoints, maxCurve,
        [&](const WaypointSegment& seg)
        {
            auto split = splitCubicBezier(seg.p0, seg.p1, seg.p2, seg.p3);
            result.left.addPath(split.left);
            result.right.addPath(split.right);
        });
    return result;
}

juce::Path CablePathBuilder::buildWaypointPath(
    const std::vector<juce::Point<float>>& waypoints,
    float maxCurve)
{
    juce::Path path;
    if (waypoints.size() < 2)
        return path;
    path.startNewSubPath(waypoints.front());
    appendWaypointSegments(waypoints, maxCurve,
        [&](const WaypointSegment& seg)
        {
            path.cubicTo(seg.p1, seg.p2, seg.p3);
        });
    return path;
}

