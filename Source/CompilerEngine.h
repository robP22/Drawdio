#pragma once
#include <cstdint>
#include <vector>
#include "PedalStructures.h"

PedalAssetPayload compileCanvas(const std::array<uint8_t, TotalCells>& gridData,
                                const std::vector<DspModuleType>& pedalSlots,
                                const std::vector<uint8_t>& manualRouting = {},
                                const std::vector<ParameterDescriptor>& existingParams = {});
