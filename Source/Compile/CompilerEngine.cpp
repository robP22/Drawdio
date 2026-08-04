#include "Compile/CompilerEngine.h"
#include "State/PedalDefinition.h"
#include "Core/CanvasAnalysis.h"
#include <algorithm>
#include <numeric>

PedalAssetPayload compileCanvas(const std::array<uint8_t, TotalCells>& gridData,
                                const std::vector<DspModuleType>& pedalSlots,
                                const std::vector<uint8_t>& manualRouting,
                                const std::vector<ParameterDescriptor>& existingParams)
{
    PedalAssetPayload result;

    // Build active chain (skip BYPASS slots), tracking original slot indices.
    std::vector<DspModuleType> activeChain;
    std::vector<uint8_t> activeSlotIndices;
    for (int i = 0; i < static_cast<int>(pedalSlots.size()); ++i)
        if (pedalSlots[static_cast<size_t>(i)] != DspModuleType::BYPASS)
        {
            activeChain.push_back(pedalSlots[static_cast<size_t>(i)]);
            activeSlotIndices.push_back(static_cast<uint8_t>(i));
        }

    if (activeChain.empty())
    {
        return result;
    }

    std::vector<DspModuleType> sortedChain;
    std::vector<uint8_t> routingSlotOrder;

    if (!manualRouting.empty())
    {
        for (uint8_t slotIdx : manualRouting)
        {
            if (slotIdx < pedalSlots.size() && pedalSlots[slotIdx] != DspModuleType::BYPASS)
            {
                routingSlotOrder.push_back(slotIdx);
                sortedChain.push_back(pedalSlots[slotIdx]);
            }
        }
    }

    // Fallback to automatic scoring if manual routing is empty or invalid
    if (routingSlotOrder.empty())
    {
        int activeCount = static_cast<int>(activeChain.size());
        auto ranges = calculateRowRanges(activeCount);

        // Score each pedal position based on painted pixel distribution
        std::vector<int> scores(activeCount, 0);
        for (int i = 0; i < activeCount; ++i)
        {
            int totalPainted = 0;
            long long xSum = 0;
            for (int y = ranges[static_cast<size_t>(i)].startRow;
                 y < ranges[static_cast<size_t>(i)].startRow + ranges[static_cast<size_t>(i)].numRows && y < GridSize; ++y)
            {
                for (int x = 0; x < GridSize; ++x)
                {
                    if (gridData[y * GridSize + x] != 0)
                    {
                        ++totalPainted;
                        xSum += x;
                    }
                }
            }
            if (totalPainted > 0)
                scores[i] = static_cast<int>((static_cast<double>(xSum) / totalPainted) * (100.0 / static_cast<double>(GridSize - 1)));
        }

        // Sort chain by routing score ascending (left-biased pixels processed first)
        std::vector<int> sortedOrder(activeCount);
        std::iota(sortedOrder.begin(), sortedOrder.end(), 0);
        std::stable_sort(sortedOrder.begin(), sortedOrder.end(),
            [&](int a, int b) { return scores[a] < scores[b]; });

        for (int idx : sortedOrder)
        {
            sortedChain.push_back(activeChain[idx]);
            routingSlotOrder.push_back(activeSlotIndices[idx]);
        }
    }

    int activeCount = static_cast<int>(sortedChain.size());
    // Recalculate row ranges for the sorted chain order
    auto sortedRanges = calculateRowRanges(activeCount);

    // Build parameter descriptors
    std::vector<ParameterDescriptor> paramDescriptors;
    for (int i = 0; i < activeCount; ++i)
    {
        const auto& definition = PedalDefinitions::get(sortedChain[static_cast<size_t>(i)]);
        for (const auto& parameter : definition.parameters)
        {
            ParameterDescriptor p;
            p.parameterToken = parameter.parameterToken;
            p.minValue = parameter.minValue;
            p.maxValue = parameter.maxValue;
            p.defaultValue = parameter.defaultValue;
            p.currentValue = parameter.defaultValue;
            p.targetDspNodeRegister = static_cast<uint8_t>(i);
            paramDescriptors.push_back(p);
        }
    }

    result.activeRoutingChain = sortedChain;
    result.routingSlotOrder = routingSlotOrder;

    for (auto& param : paramDescriptors)
    {
        ParameterDescriptor compiledParam = param;
        int chainPos = static_cast<int>(param.targetDspNodeRegister);

        // Check for manual override in existing params
        bool overridden = false;
        for (const auto& ep : existingParams)
        {
            if (ep.targetDspNodeRegister == param.targetDspNodeRegister &&
                ep.parameterToken == param.parameterToken &&
                ep.isManualOverride)
            {
                compiledParam.currentValue = ep.currentValue;
                compiledParam.isManualOverride = true;
                overridden = true;
                break;
            }
        }

        if (!overridden && chainPos >= 0 && chainPos < activeCount)
        {
            const auto& definition = PedalDefinitions::get(sortedChain[static_cast<size_t>(chainPos)]);
            const int paramCount = static_cast<int>(definition.parameters.size());
            int paramIdx = paramCount;
            for (int i = 0; i < paramCount; ++i)
            {
                if (definition.parameters[static_cast<size_t>(i)].parameterToken == param.parameterToken)
                {
                    paramIdx = i;
                    break;
                }
            }

            if (paramIdx >= paramCount)
                continue;

            float normalized = calculatePixelAccumulation(gridData,
                                   sortedRanges[static_cast<size_t>(chainPos)],
                                   paramIdx, paramCount);
            compiledParam.currentValue = param.minValue + (normalized * (param.maxValue - param.minValue));
        }

        result.parameters.push_back(compiledParam);
    }

    return result;
}
