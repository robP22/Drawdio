#include "EffectConfigRegistry.h"

ReverbNetworkConfig EffectConfigRegistry::getDiffusedReverbConfig()
{
    return ReverbNetworkConfig{
        .feedbackBase = 0.85,
        .feedbackRange = 0.12,
        .fdnTimes = { 1759, 2029, 2311, 2609, 2917, 3221, 3571, 3929 }
    };
}

ReverbNetworkConfig EffectConfigRegistry::getPlateReverbConfig()
{
    return ReverbNetworkConfig{
        .feedbackBase = 0.88,
        .feedbackRange = 0.08,
        .fdnTimes = { 1321, 1427, 1543, 1657, 1783, 1907, 2039, 2161 }
    };
}
