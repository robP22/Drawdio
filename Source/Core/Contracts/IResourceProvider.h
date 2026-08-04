#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstddef>

class IResourceProvider
{
public:
    virtual ~IResourceProvider() = default;

    enum class ImageId : size_t
    {
        WorkspaceWood = 0,
        PedalboardSprite,
        ColorPaletteBody,
        PedalEnclosure,
        PedalFaceGrain,
        OverlayGloss,
        PedalKnobImage,
        PedalLedImage,
        CanvasTexture,
        ColorWell,
        JapanesePedalSheet,
        Count
    };

    enum class TextureId : size_t
    {
        WorkspaceWood = static_cast<size_t>(ImageId::WorkspaceWood),
        PedalboardSprite = static_cast<size_t>(ImageId::PedalboardSprite),
        ColorPaletteBody = static_cast<size_t>(ImageId::ColorPaletteBody),
        PedalEnclosure = static_cast<size_t>(ImageId::PedalEnclosure),
        PedalFaceGrain = static_cast<size_t>(ImageId::PedalFaceGrain),
        OverlayGloss = static_cast<size_t>(ImageId::OverlayGloss),
        CanvasTexture = static_cast<size_t>(ImageId::CanvasTexture),
        ColorWell = static_cast<size_t>(ImageId::ColorWell),
        JapanesePedalSheet = static_cast<size_t>(ImageId::JapanesePedalSheet)
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
        PedalKnob,
        PedalLedOn,
        PedalLedOff,
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

    virtual const juce::Image& getImage(ImageId id) const = 0;
    virtual const juce::Image& getTexture(TextureId id) const = 0;
    virtual const SpriteSheet& getSpriteSheet(SpriteSheetId id) const = 0;
    virtual const SpriteFrame& getSpriteFrame(SpriteId id) const = 0;
};
