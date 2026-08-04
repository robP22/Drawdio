#include "AutomationCompiler.h"
#include "Core/CanvasAnalysis.h"

AutomationEnvelope AutomationCompiler::compile(
    const std::array<uint8_t, TotalCells>& gridData,
    const std::vector<DspModuleType>& /*pedalSlots*/)
{
    AutomationEnvelope envelope;

    constexpr int numSlices = 64;
    const int colsPerSlice = GridSize / numSlices;

    for (int slice = 0; slice < numSlices; ++slice)
    {
        int startCol = slice * colsPerSlice;
        int endCol = (slice == numSlices - 1) ? GridSize : startCol + colsPerSlice;

        float sumWeightedY = 0.0f;
        float sumWeight = 0.0f;

        for (int col = startCol; col < endCol; ++col)
            for (int row = 0; row < GridSize; ++row)
            {
                uint8_t val = gridData[row * GridSize + col];
                if (val == 0) continue;

                float w = std::abs(colorWeight(val));
                sumWeightedY += static_cast<float>(row) * w;
                sumWeight += w;
            }

        float t = static_cast<float>(slice) / static_cast<float>(numSlices - 1);
        float v = 0.5f;

        if (sumWeight > 0.0f)
        {
            float avgY = sumWeightedY / sumWeight;
            v = 1.0f - avgY / static_cast<float>(GridSize - 1);
        }

        envelope.addPoint(t, v);
    }

    return envelope;
}
