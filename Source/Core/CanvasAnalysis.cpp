#include "Core/CanvasAnalysis.h"

#include <cmath>

float colorWeight(uint8_t pixelVal)
{
    switch (pixelVal)
    {
        case 5:  return -1.0f;
        case 7:  return  0.9f;
        case 8:  return -0.55f;
        case 12: return  0.6f;
        case 1:  return -0.8f;
        case 2:  return  0.55f;
        case 9:  return  0.0f;
        case 10: return -0.6f;
        case 6:  return  0.7f;
        case 11: return -0.9f;
        case 3:  return  0.8f;
        case 4:  return -0.7f;
        default: return  0.0f;
    }
}

std::vector<PedalRowRange> calculateRowRanges(int activeCount)
{
    if (activeCount <= 0)
        return {};

    std::vector<PedalRowRange> ranges(static_cast<size_t>(activeCount));
    int baseSlice = GridSize / activeCount;
    int remainder = GridSize % activeCount;
    int currentRow = 0;
    for (int i = 0; i < activeCount; ++i)
    {
        ranges[static_cast<size_t>(i)].startRow = currentRow;
        ranges[static_cast<size_t>(i)].numRows = baseSlice + (i < remainder ? 1 : 0);
        currentRow += ranges[static_cast<size_t>(i)].numRows;
    }
    return ranges;
}
