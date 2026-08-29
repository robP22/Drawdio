#pragma once
#include <array>

#include "State/AutomationEnvelope.h"
#include "Core/DrawdioConstants.h"

class AutomationCompiler
{
public:
    AutomationEnvelope compile(const std::array<uint8_t, TotalCells>& gridData);
};
