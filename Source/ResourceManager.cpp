#include "ResourceManager.h"
#include "BinaryData.h"

#include <utility>

ResourceManager::ResourceManager()
{
    loadAll();
}

const juce::Image& ResourceManager::getImage(ImageId id) const
{
    return m_images[static_cast<size_t>(id)];
}

const juce::Image& ResourceManager::getTexture(TextureId id) const
{
    return m_images[static_cast<size_t>(id)];
}

const ResourceManager::SpriteSheet& ResourceManager::getSpriteSheet(SpriteSheetId id) const
{
    return m_spriteSheets[static_cast<size_t>(id)];
}

const ResourceManager::SpriteFrame& ResourceManager::getSpriteFrame(SpriteId id) const
{
    return m_spriteFrames[static_cast<size_t>(id)];
}

juce::Rectangle<int> ResourceManager::getSpriteSourceRect(SpriteId id) const
{
    return getSpriteFrame(id).source;
}

juce::Typeface::Ptr ResourceManager::getTypeface(FontId id) const
{
    return m_typefaces[static_cast<size_t>(id)];
}

std::unique_ptr<juce::Drawable> ResourceManager::createSvgDrawable(SvgId id) const
{
    const auto& xml = m_svgXml[static_cast<size_t>(id)];
    if (xml.isEmpty())
        return {};

    auto document = juce::XmlDocument::parse(xml);
    if (document == nullptr)
        return {};

    return juce::Drawable::createFromSVG(*document);
}

void ResourceManager::loadAll()
{
    loadProceduralTextures();
}

void ResourceManager::loadProceduralTextures()
{
    // Load REAL sprite images from binary data
    m_images[static_cast<size_t>(ImageId::WorkspaceWood)] =
        decodeImageFromBinaryData(BinaryData::wood_texture_generic_png, BinaryData::wood_texture_generic_pngSize);
    m_images[static_cast<size_t>(ImageId::PedalboardFelt)] =
        decodeImageFromBinaryData(BinaryData::pedalboard_bg_png, BinaryData::pedalboard_bg_pngSize);
    m_images[static_cast<size_t>(ImageId::PalettePaint)] =
        decodeImageFromBinaryData(BinaryData::palette_body_png, BinaryData::palette_body_pngSize);
    m_images[static_cast<size_t>(ImageId::ColorPaletteBody)] =
        decodeImageFromBinaryData(BinaryData::colorpalettebody_png, BinaryData::colorpalettebody_pngSize);
}

juce::Image ResourceManager::decodeImageFromBinaryData(const void* data, size_t sizeInBytes)
{
    if (data == nullptr || sizeInBytes == 0)
        return {};

    return juce::ImageFileFormat::loadFrom(data, sizeInBytes);
}
