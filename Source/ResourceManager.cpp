#include "ResourceManager.h"
#include "BinaryData.h"

#include <utility>

namespace
{
constexpr int kKnobFrameCount = 32;  // Number of frames in knob rotation sprite sheet
}

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
}

void ResourceManager::loadSpriteSheets()
{
    // Set up knob sprite sheet from the loaded image
    const auto& knobImage = m_images[static_cast<size_t>(ImageId::PedalKnobImage)];
    if (knobImage.isValid())
    {
        // Assume horizontal strip of frames
        const int totalWidth = knobImage.getWidth();
        const int frameWidth = totalWidth / kKnobFrameCount;
        const int frameHeight = knobImage.getHeight();

        m_spriteSheets[static_cast<size_t>(SpriteSheetId::PedalParts)].image = knobImage;
        m_spriteSheets[static_cast<size_t>(SpriteSheetId::PedalParts)].frameWidth = frameWidth;
        m_spriteSheets[static_cast<size_t>(SpriteSheetId::PedalParts)].frameHeight = frameHeight;

        // Set up PedalKnob sprite frame - frame 0 at default position
        m_spriteFrames[static_cast<size_t>(SpriteId::PedalKnob)] = {
            SpriteSheetId::PedalParts,
            juce::Rectangle<int>(0, 0, frameWidth, frameHeight)
        };

        // Set up other sprite frames (these would need actual sprite sheet data)
        // For now, placeholder frames that use the full image area
        m_spriteFrames[static_cast<size_t>(SpriteId::PedalBody)] = {
            SpriteSheetId::PedalParts,
            juce::Rectangle<int>(0, 0, frameWidth, frameHeight)
        };
        m_spriteFrames[static_cast<size_t>(SpriteId::PedalLed)] = {
            SpriteSheetId::PedalParts,
            juce::Rectangle<int>(0, 0, frameWidth, frameHeight)
        };
        m_spriteFrames[static_cast<size_t>(SpriteId::PedalJack)] = {
            SpriteSheetId::PedalParts,
            juce::Rectangle<int>(0, 0, frameWidth, frameHeight)
        };
    }
}

juce::Image ResourceManager::decodeImageFromBinaryData(const void* data, size_t sizeInBytes)
{
    if (data == nullptr || sizeInBytes == 0)
        return {};

    return juce::ImageFileFormat::loadFrom(data, sizeInBytes);
}
