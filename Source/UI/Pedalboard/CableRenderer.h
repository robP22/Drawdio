#pragma once
#include <JuceHeader.h>
#include <vector>
#include "CablePathBuilder.h"
#include "UI/Theme/IThemeProvider.h"

class CableRenderer
{
public:
    explicit CableRenderer(const IThemeProvider& theme);

    static void renderSegment(juce::Graphics& g, const juce::Path& left, const juce::Path& right,
                               juce::Colour leftBase, juce::Colour rightBase);

    void drawRoutingCables(juce::Graphics& g,
                           const std::vector<CachedSplitCable>& cables,
                           int skipGrabbedIndex) const;

    void drawActiveDraggingCable(juce::Graphics& g,
                                 juce::Point<float> start, juce::Point<float> current,
                                 int srcJackIdx) const;

    void drawGrabbedCable(juce::Graphics& g,
                          juce::Point<float> fromPos, juce::Point<float> toPos) const;

    void drawInputJack(juce::Graphics& g, juce::Point<float> entryPos,
                       const juce::Path& path) const;

    void drawOutputJack(juce::Graphics& g, juce::Point<float> exitPos,
                        const juce::Path& path) const;

private:
    const IThemeProvider& m_theme;
};
