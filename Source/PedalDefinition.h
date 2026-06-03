#pragma once

#include <JuceHeader.h>
#include <array>

#include "PedalStructures.h"
#include "ResourceManager.h"

struct NormalizedControlBounds
{
    float centreX = 0.5f;
    float centreY = 0.5f;
    float width = 0.25f;
    float height = 0.20f;
};

struct PedalParameterDefinition
{
    uint16_t parameterToken = 0;
    const char* label = "Param";
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.5f;
};

struct PedalSpriteMetadata
{
    ResourceManager::SpriteId body = ResourceManager::SpriteId::PedalBody;
    ResourceManager::SpriteId face = ResourceManager::SpriteId::PedalFace;
    ResourceManager::SpriteId lcd = ResourceManager::SpriteId::PedalLcd;
    ResourceManager::SpriteId led = ResourceManager::SpriteId::PedalLed;
    ResourceManager::SpriteId jack = ResourceManager::SpriteId::PedalJack;
};

struct PedalDefinition
{
    DspModuleType type = DspModuleType::BYPASS;
    const char* displayName = "Unknown";
    PedalSpriteMetadata sprites;
    std::array<NormalizedControlBounds, 4> knobLayout;
    std::array<PedalParameterDefinition, 4> parameters;
};

namespace PedalDefinitions
{
    const PedalDefinition& get(DspModuleType type);
    const PedalDefinition& fallback();
    juce::String getDisplayName(DspModuleType type);
    juce::String getParameterLabel(DspModuleType type, int parameterIndex);
}
