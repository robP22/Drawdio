#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstddef>
#include <memory>

class ResourceManager final
{
public:
    enum class ImageId : size_t
    {
        WorkspaceWood = 0,
        PedalboardSprite,
        ColorPaletteBody,
        PedalEnclosure,
        PedalFaceGrain,
        OverlayGloss,
        Count
    };

    enum class TextureId : size_t
    {
        WorkspaceWood = static_cast<size_t>(ImageId::WorkspaceWood),
        PedalboardSprite = static_cast<size_t>(ImageId::PedalboardSprite),
        ColorPaletteBody = static_cast<size_t>(ImageId::ColorPaletteBody),
        PedalEnclosure = static_cast<size_t>(ImageId::PedalEnclosure),
        PedalFaceGrain = static_cast<size_t>(ImageId::PedalFaceGrain),
        OverlayGloss = static_cast<size_t>(ImageId::OverlayGloss)
    };

    enum class SpriteSheetId : size_t
    {
        PedalParts = 0,
        Buttons,
        Meters,
        Overlays,
        JapanesePedals,
        GeneralPedals,
        Count
    };

    enum class SpriteId : size_t
    {
        PedalBody = 0,
        PedalFace,
        PedalLcd,
        PedalLed,
        PedalJack,
        ButtonDefault,
        MeterSegment,
        OverlayGloss,
        // Japanese skin sprites
        JapPedalBody,
        JapPedalLed,
        JapPedalJack,
        // General skin sprites
        GenPedalBody,
        GenPedalLed,
        GenPedalJack,
        Count
    };

    enum class FontId : size_t
    {
        Default = 0,
        Count
    };

    enum class SvgId : size_t
    {
        Placeholder = 0,
        Count
    };

    struct SpriteSheet
    {
        juce::Image image;
        int frameWidth = 0;
        int frameHeight = 0;
    };

    struct SpriteFrame
    {
        SpriteSheetId sheetId = SpriteSheetId::PedalParts;
        juce::Rectangle<int> source;
    };

    ResourceManager();

    const juce::Image& getImage(ImageId id) const;
    const juce::Image& getTexture(TextureId id) const;
    const SpriteSheet& getSpriteSheet(SpriteSheetId id) const;
    const SpriteFrame& getSpriteFrame(SpriteId id) const;
    juce::Rectangle<int> getSpriteSourceRect(SpriteId id) const;
    juce::Typeface::Ptr getTypeface(FontId id) const;
    std::unique_ptr<juce::Drawable> createSvgDrawable(SvgId id) const;

private:
    void loadAll();
    void loadProceduralTextures();

    static juce::Image decodeImageFromBinaryData(const void* data, size_t sizeInBytes);

    std::array<juce::Image, static_cast<size_t>(ImageId::Count)> m_images;
    std::array<SpriteSheet, static_cast<size_t>(SpriteSheetId::Count)> m_spriteSheets;
    std::array<SpriteFrame, static_cast<size_t>(SpriteId::Count)> m_spriteFrames;
    std::array<juce::Typeface::Ptr, static_cast<size_t>(FontId::Count)> m_typefaces;
    std::array<juce::String, static_cast<size_t>(SvgId::Count)> m_svgXml;
};
