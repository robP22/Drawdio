#include "ScaledAssetProvider.h"

ScaledAssetProvider::ScaledAssetProvider(const IResourceProvider& resources)
    : m_resources(resources)
{
}

void ScaledAssetProvider::setResizeActive(bool active) const
{
    m_resizeActive = active;
}

juce::Image ScaledAssetProvider::getScaledImage(IResourceProvider::ImageId id,
                                                int width,
                                                int height,
                                                ResamplingPolicy policy) const
{
    return getScaledFrame(id, {}, width, height, policy);
}

juce::Image ScaledAssetProvider::getScaledFrame(IResourceProvider::ImageId id,
                                                juce::Rectangle<int> source,
                                                int width,
                                                int height,
                                                ResamplingPolicy policy) const
{
    if (width <= 0 || height <= 0)
        return {};

    const bool fullSource = source.isEmpty();
    ++m_useCounter;
    for (auto& entry : m_cache)
    {
        if (entry.id == id && entry.width == width && entry.height == height
            && entry.policy == policy && entry.fullSource == fullSource
            && (fullSource || entry.source == source) && entry.image.isValid())
        {
            entry.lastUsed = m_useCounter;
            return entry.image;
        }
    }

    const auto& sourceImage = m_resources.getImage(id);
    if (!sourceImage.isValid())
        return {};

    const auto sourceBounds = fullSource ? sourceImage.getBounds() : source;

    if (m_resizeActive)
    {
        CacheEntry* best = nullptr;
        for (auto& entry : m_cache)
            if (entry.id == id && entry.source == sourceBounds
                && entry.policy == policy && entry.fullSource == fullSource
                && entry.image.isValid()
                && (best == nullptr || entry.lastUsed > best->lastUsed))
                best = &entry;
        if (best != nullptr)
        {
            best->lastUsed = m_useCounter;
            return best->image;
        }
    }

    const auto quality = policy == ResamplingPolicy::PixelArt
        ? juce::Graphics::lowResamplingQuality
        : juce::Graphics::highResamplingQuality;
    auto* entry = &m_cache[0];
    for (auto& candidate : m_cache)
        if (!candidate.image.isValid() || candidate.lastUsed < entry->lastUsed)
            entry = &candidate;

    entry->id = id;
    entry->image = sourceImage.getClippedImage(sourceBounds).rescaled(width, height, quality);
    entry->width = width;
    entry->height = height;
    entry->source = sourceBounds;
    entry->policy = policy;
    entry->lastUsed = m_useCounter;
    entry->fullSource = fullSource;
    return entry->image;
}

void ScaledAssetProvider::drawImage(juce::Graphics& graphics,
                                    IResourceProvider::ImageId id,
                                    juce::Rectangle<float> destination,
                                    ResamplingPolicy policy) const
{
    if (destination.isEmpty())
        return;

    const int width = juce::jmax(1, juce::roundToInt(destination.getWidth()));
    const int height = juce::jmax(1, juce::roundToInt(destination.getHeight()));
    const auto image = getScaledImage(id, width, height, policy);
    if (!image.isValid())
        return;

    graphics.saveState();
    graphics.setImageResamplingQuality(policy == ResamplingPolicy::PixelArt
        ? juce::Graphics::lowResamplingQuality
        : juce::Graphics::highResamplingQuality);
    if (image.getWidth() == width && image.getHeight() == height
        && destination.getX() == static_cast<float>(juce::roundToInt(destination.getX()))
        && destination.getY() == static_cast<float>(juce::roundToInt(destination.getY())))
    {
        graphics.drawImageAt(image, juce::roundToInt(destination.getX()),
                             juce::roundToInt(destination.getY()));
    }
    else
    {
        graphics.drawImage(image, destination,
                           juce::RectanglePlacement::stretchToFit, false);
    }
    graphics.restoreState();
}

void ScaledAssetProvider::drawFrame(juce::Graphics& graphics,
                                    IResourceProvider::ImageId id,
                                    juce::Rectangle<int> sourceBounds,
                                    juce::Rectangle<float> destination,
                                    ResamplingPolicy policy) const
{
    if (sourceBounds.isEmpty() || destination.isEmpty())
        return;

    const int width = juce::jmax(1, juce::roundToInt(destination.getWidth()));
    const int height = juce::jmax(1, juce::roundToInt(destination.getHeight()));
    const auto image = getScaledFrame(id, sourceBounds, width, height, policy);
    if (!image.isValid())
        return;

    graphics.saveState();
    graphics.setImageResamplingQuality(policy == ResamplingPolicy::PixelArt
        ? juce::Graphics::lowResamplingQuality
        : juce::Graphics::highResamplingQuality);
    if (image.getWidth() == width && image.getHeight() == height
        && destination.getX() == static_cast<float>(juce::roundToInt(destination.getX()))
        && destination.getY() == static_cast<float>(juce::roundToInt(destination.getY())))
    {
        graphics.drawImageAt(image, juce::roundToInt(destination.getX()),
                             juce::roundToInt(destination.getY()));
    }
    else
    {
        graphics.drawImage(image, destination,
                           juce::RectanglePlacement::stretchToFit, false);
    }
    graphics.restoreState();
}
