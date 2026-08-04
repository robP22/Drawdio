#pragma once

#include "Dsp/ReverbNetwork.h"

class EffectConfigRegistry
{
public:
    static ReverbNetworkConfig getDiffusedReverbConfig();
    static ReverbNetworkConfig getPlateReverbConfig();
};
