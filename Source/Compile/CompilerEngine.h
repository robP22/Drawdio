#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "Core/CompiledPedalConfig.h"
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"

PedalAssetPayload compileCanvas(const std::array<uint8_t, TotalCells>& gridData,
                                const std::vector<DspModuleType>& pedalSlots,
                                const std::vector<uint8_t>& manualRouting = {},
                                const std::vector<ParameterDescriptor>& existingParams = {});
