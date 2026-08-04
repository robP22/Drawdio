#pragma once
#include <JuceHeader.h>
#include "Core/Contracts/IResourceProvider.h"

class WoodGrainBackground : public juce::Component
{
public:
    WoodGrainBackground(const IResourceProvider& resources)
        : m_resources(resources) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto& woodTexture = m_resources.getTexture(IResourceProvider::TextureId::WorkspaceWood);
        if (woodTexture.isValid())
            g.drawImage(woodTexture, bounds, juce::RectanglePlacement::stretchToFit);
    }
private:
    const IResourceProvider& m_resources;
};
