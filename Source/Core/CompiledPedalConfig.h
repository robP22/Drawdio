#pragma once

#include <cstdint>
#include <vector>

#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"

struct PedalAssetPayload
{
    std::vector<DspModuleType> activeRoutingChain;
    std::vector<uint8_t> routingSlotOrder;
    std::vector<ParameterDescriptor> parameters;
};
