#pragma once
#include <JuceHeader.h>
#include "Core/Contracts/IResourceProvider.h"

class PedalboardBackground : public juce::Component
{
public:
    PedalboardBackground(const IResourceProvider& resources)
        : m_resources(resources) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto& texture = m_resources.getTexture(IResourceProvider::TextureId::PedalboardSprite);
        if (texture.isValid())
            g.drawImage(texture, bounds.getX(), bounds.getY(),
                       bounds.getWidth(), bounds.getHeight(),
                       0, 0, texture.getWidth(), texture.getHeight());
    }
private:
    const IResourceProvider& m_resources;
};
