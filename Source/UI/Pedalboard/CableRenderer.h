#pragma once
#include <JuceHeader.h>
#include <vector>
#include "CablePathBuilder.h"
#include "Core/Contracts/IResourceProvider.h"
#include "Resources/ScaledAssetProvider.h"
#include "UI/Theme/IThemeProvider.h"

class CableRenderer
{
public:
    explicit CableRenderer(const IThemeProvider& theme,
                           const IResourceProvider& resources,
                           const ScaledAssetProvider& assets);

    static void renderSegment(juce::Graphics& g, const juce::Path& left, const juce::Path& right,
                               juce::Colour base);

    void drawRoutingCables(juce::Graphics& g,
                           const std::vector<CachedSplitCable>& cables,
                           int skipGrabbedIndex) const;

    void drawActiveDraggingCable(juce::Graphics& g,
                                 juce::Point<float> start, juce::Point<float> current,
                                 int srcJackIdx) const;

    void drawGrabbedCable(juce::Graphics& g,
                          juce::Point<float> fromPos, juce::Point<float> toPos) const;

    void drawInputJack(juce::Graphics& g, juce::Point<float> entryPos,
                       const juce::Path& path, bool drawPath) const;

    void drawOutputJack(juce::Graphics& g, juce::Point<float> exitPos,
                        const juce::Path& path, bool drawPath) const;

    void drawJackHighlight(juce::Graphics& g, juce::Point<float> position) const;

private:
    void drawJack(juce::Graphics& g, juce::Point<float> position,
                  const juce::Path& path, bool isInput, bool drawPath) const;

    const IThemeProvider& m_theme;
    const IResourceProvider& m_resources;
    const ScaledAssetProvider& m_assets;
};
