#pragma once

#include <JuceHeader.h>
#include <array>

#include "Core/Contracts/IResourceProvider.h"

class ScaledAssetProvider
{
public:
    enum class ResamplingPolicy
    {
        PixelArt,
        Continuous
    };

    explicit ScaledAssetProvider(const IResourceProvider& resources);

    void setResizeActive(bool active) const;
    bool isResizeActive() const { return m_resizeActive; }

    juce::Image getScaledImage(IResourceProvider::ImageId id,
                               int width,
                               int height,
                               ResamplingPolicy policy) const;

    juce::Image getScaledFrame(IResourceProvider::ImageId id,
                               juce::Rectangle<int> source,
                               int width,
                               int height,
                               ResamplingPolicy policy) const;

    void drawImage(juce::Graphics& graphics,
                   IResourceProvider::ImageId id,
                   juce::Rectangle<float> destination,
                   ResamplingPolicy policy) const;

    void drawFrame(juce::Graphics& graphics,
                   IResourceProvider::ImageId id,
                   juce::Rectangle<int> source,
                   juce::Rectangle<float> destination,
                   ResamplingPolicy policy) const;

private:
    struct CacheEntry
    {
        IResourceProvider::ImageId id = IResourceProvider::ImageId::Count;
        juce::Image image;
        int width = 0;
        int height = 0;
        juce::Rectangle<int> source;
        ResamplingPolicy policy = ResamplingPolicy::Continuous;
        uint64_t lastUsed = 0;
        bool fullSource = false;
    };

    const IResourceProvider& m_resources;
    mutable std::array<CacheEntry, 24> m_cache;
    mutable uint64_t m_useCounter = 0;
    mutable bool m_resizeActive = false;
};
