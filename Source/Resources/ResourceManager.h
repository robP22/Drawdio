#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstddef>
#include <memory>
#include "Core/Contracts/IResourceProvider.h"

class ResourceManager final : public IResourceProvider
{
public:
    ResourceManager();

    const juce::Image& getImage(ImageId id) const override;
    const juce::Image& getTexture(TextureId id) const override;
    const SpriteSheet& getSpriteSheet(SpriteSheetId id) const override;
    const SpriteFrame& getSpriteFrame(SpriteId id) const override;

private:
    void loadAll();
    void loadProceduralTextures();
    void loadSpriteSheets();

    static juce::Image decodeImageFromBinaryData(const void* data, size_t sizeInBytes);

    std::array<juce::Image, static_cast<size_t>(ImageId::Count)> m_images;
    std::array<SpriteSheet, static_cast<size_t>(SpriteSheetId::Count)> m_spriteSheets;
    std::array<SpriteFrame, static_cast<size_t>(SpriteId::Count)> m_spriteFrames;
};
