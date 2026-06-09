#include "EffectConfigRegistry.h"

ReverbNetworkConfig EffectConfigRegistry::getDiffusedReverbConfig()
{
    return ReverbNetworkConfig{
        .feedbackBase = 0.75,
        .feedbackRange = 0.15,
        .combGains = { 0.82f, 0.78f, 0.85f, 0.80f },
        .apCoeff = 0.5f,
        .combTimesMs = { 1557, 1617, 1491, 1373 },
        .apTimesMs = { 225, 556 }
    };
}

ReverbNetworkConfig EffectConfigRegistry::getPlateReverbConfig()
{
    return ReverbNetworkConfig{
        .feedbackBase = 0.82,
        .feedbackRange = 0.10,
        .combGains = { 0.88f, 0.85f, 0.90f, 0.87f },
        .apCoeff = 0.65f,
        .combTimesMs = { 1223, 1277, 1189, 1355 },
        .apTimesMs = { 178, 467 }
    };
}