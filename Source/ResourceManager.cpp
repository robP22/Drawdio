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
    loadSpriteSheets();
}

void ResourceManager::loadProceduralTextures()
{
    // Load REAL sprite images from binary data
    m_images[static_cast<size_t>(ImageId::WorkspaceWood)] =
        decodeImageFromBinaryData(BinaryData::wood_roundover_alpha_fixed_png, BinaryData::wood_roundover_alpha_fixed_pngSize);
    m_images[static_cast<size_t>(ImageId::PedalboardSprite)] =
        decodeImageFromBinaryData(BinaryData::pedalboard_final_png, BinaryData::pedalboard_final_pngSize);
    m_images[static_cast<size_t>(ImageId::ColorPaletteBody)] =
        decodeImageFromBinaryData(BinaryData::colorpalette_final_png, BinaryData::colorpalette_final_pngSize);
    m_images[static_cast<size_t>(ImageId::PedalEnclosure)] =
        decodeImageFromBinaryData(BinaryData::pedalenclosure_final_png, BinaryData::pedalenclosure_final_pngSize);
    m_images[static_cast<size_t>(ImageId::PedalKnobImage)] =
        decodeImageFromBinaryData(BinaryData::Knob_Generic_alpha_cutout_png, BinaryData::Knob_Generic_alpha_cutout_pngSize);
    m_images[static_cast<size_t>(ImageId::PedalLedImage)] =
        decodeImageFromBinaryData(BinaryData::ledonoff_png, BinaryData::ledonoff_pngSize);
    m_images[static_cast<size_t>(ImageId::CanvasTexture)] =
        decodeImageFromBinaryData(BinaryData::canvastexture_png, BinaryData::canvastexture_pngSize);
    m_images[static_cast<size_t>(ImageId::ColorWell)] =
        decodeImageFromBinaryData(BinaryData::colorwell_png, BinaryData::colorwell_pngSize);
    m_images[static_cast<size_t>(ImageId::JapanesePedalSheet)] =
        decodeImageFromBinaryData(BinaryData::jap_pedal_sprite_sheet_png, BinaryData::jap_pedal_sprite_sheet_pngSize);
}

void ResourceManager::loadSpriteSheets()
{
    const auto& knobImage = m_images[static_cast<size_t>(ImageId::PedalKnobImage)];
    if (knobImage.isValid())
    {
        m_spriteSheets[static_cast<size_t>(SpriteSheetId::PedalParts)].image = knobImage;
        m_spriteSheets[static_cast<size_t>(SpriteSheetId::PedalParts)].frameWidth = knobImage.getWidth();
        m_spriteSheets[static_cast<size_t>(SpriteSheetId::PedalParts)].frameHeight = knobImage.getHeight();

        m_spriteFrames[static_cast<size_t>(SpriteId::PedalKnob)] = {
            SpriteSheetId::PedalParts,
            knobImage.getBounds()
        };

        m_spriteFrames[static_cast<size_t>(SpriteId::PedalBody)] = {
            SpriteSheetId::PedalParts,
            knobImage.getBounds()
        };
        m_spriteFrames[static_cast<size_t>(SpriteId::PedalJack)] = {
            SpriteSheetId::PedalParts,
            knobImage.getBounds()
        };
    }

    const auto& ledImage = m_images[static_cast<size_t>(ImageId::PedalLedImage)];
    if (ledImage.isValid())
    {
        const int frameW = ledImage.getWidth() / 2;
        const int frameH = ledImage.getHeight();
        m_spriteFrames[static_cast<size_t>(SpriteId::PedalLedOff)] = {
            SpriteSheetId::PedalParts,
            juce::Rectangle<int>(0, 0, frameW, frameH)
        };
        m_spriteFrames[static_cast<size_t>(SpriteId::PedalLedOn)] = {
            SpriteSheetId::PedalParts,
            juce::Rectangle<int>(frameW, 0, frameW, frameH)
        };
    }
}

juce::Image ResourceManager::decodeImageFromBinaryData(const void* data, size_t sizeInBytes)
{
    if (data == nullptr || sizeInBytes == 0)
        return {};

    return juce::ImageFileFormat::loadFrom(data, sizeInBytes);
}
