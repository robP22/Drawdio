#pragma once

#include <array>
#include <cstdint>

#include "Core/CanvasAnalysis.h"
#include "Core/DrawdioConstants.h"

class CanvasGraphAnalyzer
{
public:
    void update(const std::array<uint8_t, TotalCells>& grid,
                const DirtyRowMask& dirtyRows,
                uint32_t revision);

    float pixelAccumulation(const PedalRowRange& range,
                            int parameterIndex,
                            int totalParameters) const;
    int routingScore(const PedalRowRange& range) const;

private:
    struct RowSummary
    {
        std::array<uint16_t, GridSize + 1> paintedPrefix{};
        std::array<int, GridSize + 1> xSumPrefix{};
        std::array<float, GridSize + 1> weightPrefix{};
        std::array<float, GridSize + 1> absWeightPrefix{};
        std::array<std::array<uint16_t, 13>, GridSize + 1> colourPrefix{};
    };

    void rebuildRow(int row);

    std::array<uint8_t, TotalCells> m_grid{};
    std::array<RowSummary, GridSize> m_rows{};
    uint32_t m_revision = 0;
    bool m_initialised = false;
};
