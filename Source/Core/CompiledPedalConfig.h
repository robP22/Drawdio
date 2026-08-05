#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"
#include "Effects/DspEffect.h"

struct PedalAssetPayload
{
    std::vector<DspModuleType> activeRoutingChain;
    std::vector<uint8_t> routingSlotOrder;
    std::vector<ParameterDescriptor> parameters;

    // F1: effect instances + per-slot param pointers. Prebuilt by the UI thread
    // before the payload is published; immutable (const) once visible to the
    // audio thread. Ownership travels with the config through the release queue.
    std::array<std::unique_ptr<DspEffect>, PedalSlotCount> effects;
    std::array<std::array<const float*, 4>, PedalSlotCount> paramPtrs{};
};
