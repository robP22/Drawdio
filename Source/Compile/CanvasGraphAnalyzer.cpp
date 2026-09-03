#include "CanvasGraphAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

void CanvasGraphAnalyzer::update(const std::array<uint8_t, TotalCells>& grid,
                                 const DirtyRowMask& dirtyRows,
                                 uint32_t revision)
{
    bool rebuildAll = !m_initialised;
    if (!rebuildAll && revision != 0 && m_revision != 0 && revision != m_revision + 1)
        rebuildAll = true;

    if (rebuildAll)
    {
        m_grid = grid;
        for (int row = 0; row < GridSize; ++row)
            rebuildRow(row);
    }
    else
    {
        for (int row = 0; row < GridSize; ++row)
        {
            if ((dirtyRows[static_cast<size_t>(row / 64)] & (uint64_t{ 1 } << (row % 64))) == 0)
                continue;

            std::memcpy(m_grid.data() + static_cast<size_t>(row * GridSize),
                        grid.data() + static_cast<size_t>(row * GridSize),
                        static_cast<size_t>(GridSize));
            rebuildRow(row);
        }
    }

    m_revision = revision;
    m_initialised = true;
}

void CanvasGraphAnalyzer::rebuildRow(int row)
{
    auto& summary = m_rows[static_cast<size_t>(row)];
    summary = {};

    for (int x = 0; x < GridSize; ++x)
    {
        const uint8_t value = m_grid[static_cast<size_t>(row * GridSize + x)];
        summary.paintedPrefix[static_cast<size_t>(x + 1)] = summary.paintedPrefix[static_cast<size_t>(x)];
        summary.xSumPrefix[static_cast<size_t>(x + 1)] = summary.xSumPrefix[static_cast<size_t>(x)];
        summary.weightPrefix[static_cast<size_t>(x + 1)] = summary.weightPrefix[static_cast<size_t>(x)];
        summary.absWeightPrefix[static_cast<size_t>(x + 1)] = summary.absWeightPrefix[static_cast<size_t>(x)];
        summary.colourPrefix[static_cast<size_t>(x + 1)] = summary.colourPrefix[static_cast<size_t>(x)];

        if (value != 0)
        {
            ++summary.paintedPrefix[static_cast<size_t>(x + 1)];
            summary.xSumPrefix[static_cast<size_t>(x + 1)] += x;
            summary.weightPrefix[static_cast<size_t>(x + 1)] += colorWeight(value);
            summary.absWeightPrefix[static_cast<size_t>(x + 1)] += std::abs(colorWeight(value));
        }

        if (value < summary.colourPrefix[static_cast<size_t>(x + 1)].size())
            ++summary.colourPrefix[static_cast<size_t>(x + 1)][value];
    }
}

float CanvasGraphAnalyzer::pixelAccumulation(const PedalRowRange& range,
                                             int parameterIndex,
                                             int totalParameters) const
{
    if (range.numRows == 0 || totalParameters == 0)
        return 0.5f;

    const int cellsPerParam = (GridSize * range.numRows) / totalParameters;
    const int startCell = parameterIndex * cellsPerParam;
    const int endCell = (parameterIndex == totalParameters - 1)
                          ? GridSize * range.numRows
                          : startCell + cellsPerParam;

    int totalCells = 0;
    int paintedCount = 0;
    float accumulator = 0.0f;
    float sumAbs = 0.0f;
    std::array<bool, 13> colourPresent{};

    for (int localStart = startCell; localStart < endCell;)
    {
        const int rowOffset = localStart / GridSize;
        const int row = range.startRow + rowOffset;
        if (row >= GridSize)
            break;

        const int rowEnd = std::min(endCell, (rowOffset + 1) * GridSize);
        const int x0 = localStart % GridSize;
        const int x1 = rowEnd % GridSize == 0 ? GridSize : rowEnd % GridSize;
        const auto& summary = m_rows[static_cast<size_t>(row)];
        const auto begin = static_cast<size_t>(x0);
        const auto end = static_cast<size_t>(x1);

        totalCells += x1 - x0;
        paintedCount += static_cast<int>(summary.paintedPrefix[end] - summary.paintedPrefix[begin]);
        accumulator += summary.weightPrefix[end] - summary.weightPrefix[begin];
        sumAbs += summary.absWeightPrefix[end] - summary.absWeightPrefix[begin];

        for (int colour = 1; colour <= 12; ++colour)
            if (summary.colourPrefix[end][static_cast<size_t>(colour)]
                != summary.colourPrefix[begin][static_cast<size_t>(colour)])
                colourPresent[static_cast<size_t>(colour)] = true;

        localStart = rowEnd;
    }

    if (totalCells == 0 || paintedCount == 0)
        return 0.5f;

    int uniqueCount = 0;
    for (int colour = 1; colour <= 12; ++colour)
        if (colourPresent[static_cast<size_t>(colour)])
            ++uniqueCount;

    const float coverage = static_cast<float>(paintedCount) / static_cast<float>(totalCells);
    const float avgWeight = accumulator / static_cast<float>(paintedCount);
    const float avgAbs = sumAbs / static_cast<float>(paintedCount);
    const float diversity = static_cast<float>(uniqueCount) / 12.0f;
    const float bias = 0.5f + avgWeight * (0.25f + diversity * 0.5f + avgAbs * 0.25f);
    const float result = bias * coverage + 0.5f * (1.0f - coverage);
    return std::clamp(result, 0.0f, 1.0f);
}

int CanvasGraphAnalyzer::routingScore(const PedalRowRange& range) const
{
    int totalPainted = 0;
    long long xSum = 0;
    for (int row = range.startRow; row < range.startRow + range.numRows && row < GridSize; ++row)
    {
        const auto& summary = m_rows[static_cast<size_t>(row)];
        const int painted = summary.paintedPrefix[GridSize];
        totalPainted += painted;
        xSum += summary.xSumPrefix[GridSize];
    }

    if (totalPainted == 0)
        return 0;
    return static_cast<int>((static_cast<double>(xSum) / totalPainted)
                            * (100.0 / static_cast<double>(GridSize - 1)));
}
