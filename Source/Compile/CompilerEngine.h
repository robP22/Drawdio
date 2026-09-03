#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "Core/CompiledPedalConfig.h"
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"
#include "Compile/CanvasGraphAnalyzer.h"

PedalAssetPayload compileCanvas(const std::array<uint8_t, TotalCells>& gridData,
                                const std::vector<DspModuleType>& pedalSlots,
                                const std::vector<uint8_t>& manualRouting = {},
                                const std::vector<ParameterDescriptor>& existingParams = {});

PedalAssetPayload compileCanvas(CanvasGraphAnalyzer& analyzer,
                                const std::array<uint8_t, TotalCells>& gridData,
                                const DirtyRowMask& dirtyRows,
                                uint32_t revision,
                                const std::vector<DspModuleType>& pedalSlots,
                                const std::vector<uint8_t>& manualRouting = {},
                                const std::vector<ParameterDescriptor>& existingParams = {});
