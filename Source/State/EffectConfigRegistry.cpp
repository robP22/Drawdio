#include "EffectConfigRegistry.h"

ReverbNetworkConfig EffectConfigRegistry::getDiffusedReverbConfig()
{
    return ReverbNetworkConfig{
        .feedbackBase = 0.80,
        .feedbackRange = 0.14,
        .fdnTimesMs = { 1627, 1669, 1753, 1811, 1913, 1999, 2053, 2141 }
    };
}

ReverbNetworkConfig EffectConfigRegistry::getPlateReverbConfig()
{
    return ReverbNetworkConfig{
        .feedbackBase = 0.83,
        .feedbackRange = 0.11,
        .fdnTimesMs = { 1291, 1327, 1361, 1429, 1481, 1531, 1571, 1613 }
    };
}
