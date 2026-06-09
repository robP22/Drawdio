#pragma once

#include "PedalStructures.h"

class EffectConfigRegistry
{
public:
    static ReverbNetworkConfig getDiffusedReverbConfig();
    static ReverbNetworkConfig getPlateReverbConfig();
};