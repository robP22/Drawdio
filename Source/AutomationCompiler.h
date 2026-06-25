#pragma once
#include "PedalStructures.h"
#include "AutomationEnvelope.h"

class AutomationCompiler
{
public:
    AutomationEnvelope compile(const std::array<uint8_t, TotalCells>& gridData,
                               const std::vector<DspModuleType>& pedalSlots);
};
