#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Core/DrawdioConstants.h"

struct PedalRowRange
{
    int startRow;
    int numRows;
};

float colorWeight(uint8_t pixelVal);
std::vector<PedalRowRange> calculateRowRanges(int activeCount);
float calculatePixelAccumulation(const std::array<uint8_t, TotalCells>& gridData,
                                 const PedalRowRange& range,
                                 int parameterIndex,
                                 int totalParameters);
