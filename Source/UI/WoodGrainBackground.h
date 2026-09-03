#pragma once
#include <JuceHeader.h>
#include "Resources/ScaledAssetProvider.h"

class WoodGrainBackground : public juce::Component
{
public:
    explicit WoodGrainBackground(const ScaledAssetProvider& assets)
        : m_assets(assets) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        m_assets.drawImage(g, IResourceProvider::ImageId::WorkspaceWood, bounds,
                           ScaledAssetProvider::ResamplingPolicy::Continuous);
    }
private:
    const ScaledAssetProvider& m_assets;
};
