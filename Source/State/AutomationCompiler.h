#pragma once
#include <array>
#include <vector>

#include "State/AutomationEnvelope.h"
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"

class AutomationCompiler
{
public:
    AutomationEnvelope compile(const std::array<uint8_t, TotalCells>& gridData,
                               const std::vector<DspModuleType>& pedalSlots);
};
