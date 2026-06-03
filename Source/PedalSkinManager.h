#pragma once
#include <JuceHeader.h>
#include <string>
#include "ResourceManager.h"

class PedalSkinManager
{
public:
    enum class PedalSkin
    {
        Default,
        Japanese,
        General
    };

    static const char* skinName(PedalSkin skin);
    static PedalSkin skinFromName(const juce::String& name);

    static ResourceManager::SpriteSheetId getSkinSpriteSheetId(PedalSkin skin);
    static ResourceManager::SpriteId getLedSpriteId(PedalSkin skin);
    static ResourceManager::SpriteId getJackSpriteId(PedalSkin skin);
};