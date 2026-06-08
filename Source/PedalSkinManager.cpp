#include "PedalSkinManager.h"

const char* PedalSkinManager::skinName(PedalSkin skin)
{
    switch (skin)
    {
        case PedalSkin::Japanese: return "Japanese";
        case PedalSkin::General:  return "General";
        case PedalSkin::Default:
        default:                  return "Default";
    }
}

PedalSkinManager::PedalSkin PedalSkinManager::skinFromName(const juce::String& name)
{
    if (name == "Japanese") return PedalSkin::Japanese;
    if (name == "General")  return PedalSkin::General;
    return PedalSkin::Default;
}

ResourceManager::SpriteSheetId PedalSkinManager::getSkinSpriteSheetId(PedalSkin skin)
{
    switch (skin)
    {
        case PedalSkin::Japanese: return ResourceManager::SpriteSheetId::JapanesePedals;
        case PedalSkin::General:  return ResourceManager::SpriteSheetId::GeneralPedals;
        case PedalSkin::Default:
        default:                  return ResourceManager::SpriteSheetId::PedalParts;
    }
}

ResourceManager::SpriteId PedalSkinManager::getLedSpriteId(PedalSkin skin)
{
    switch (skin)
    {
        case PedalSkin::Japanese: return ResourceManager::SpriteId::JapPedalLed;
        case PedalSkin::General:  return ResourceManager::SpriteId::GenPedalLed;
        case PedalSkin::Default:
        default:                  return ResourceManager::SpriteId::PedalLed;
    }
}

ResourceManager::SpriteId PedalSkinManager::getJackSpriteId(PedalSkin skin)
{
    switch (skin)
    {
        case PedalSkin::Japanese: return ResourceManager::SpriteId::JapPedalJack;
        case PedalSkin::General:  return ResourceManager::SpriteId::GenPedalJack;
        case PedalSkin::Default:
        default:                  return ResourceManager::SpriteId::PedalJack;
    }
}

ResourceManager::SpriteId PedalSkinManager::getKnobSpriteId(PedalSkin)
{
    // All skins use the same generic knob sprite for now
    return ResourceManager::SpriteId::PedalKnob;
}