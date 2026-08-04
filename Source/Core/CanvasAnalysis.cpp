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

float calculatePixelAccumulation(const std::array<uint8_t, TotalCells>& gridData,
                                 const PedalRowRange& range,
                                 int parameterIndex,
                                 int totalParameters)
{
    if (range.numRows == 0 || totalParameters == 0)
        return 0.5f;

    int cellsPerParam = (GridSize * range.numRows) / totalParameters;
    int startCell = parameterIndex * cellsPerParam;
    int endCell = (parameterIndex == totalParameters - 1)
                      ? (GridSize * range.numRows)
                      : (startCell + cellsPerParam);

    float accumulator = 0.0f;
    float sumAbs = 0.0f;
    int paintedCount = 0;
    int totalCells = 0;
    bool colorPresent[13] = {false};

    for (int localCell = startCell; localCell < endCell; ++localCell)
    {
        int gridX = localCell % GridSize;
        int gridY = range.startRow + (localCell / GridSize);
        if (gridY >= GridSize)
            break;

        uint8_t val = gridData[gridY * GridSize + gridX];
        ++totalCells;
        if (val != 0)
        {
            ++paintedCount;
            float w = colorWeight(val);
            accumulator += w;
            sumAbs += std::abs(w);
            colorPresent[val] = true;
        }
    }

    if (totalCells == 0 || paintedCount == 0)
        return 0.5f;

    int uniqueCount = 0;
    for (int i = 1; i <= 12; ++i)
        if (colorPresent[i])
            ++uniqueCount;

    float coverage = static_cast<float>(paintedCount) / static_cast<float>(totalCells);
    float avgWeight = accumulator / static_cast<float>(paintedCount);
    float avgAbs = sumAbs / static_cast<float>(paintedCount);
    float diversity = static_cast<float>(uniqueCount) / 12.0f;
    float bias = 0.5f + avgWeight * (0.25f + diversity * 0.5f + avgAbs * 0.25f);
    float result = bias * coverage + 0.5f * (1.0f - coverage);

    return (result < 0.0f) ? 0.0f : (result > 1.0f ? 1.0f : result);
}
