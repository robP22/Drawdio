#include "Compile/CompilerEngine.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <numeric>

#include "State/PedalDefinition.h"
#include "Effects/DspEffect.h"

namespace
{
PedalAssetPayload compileCanvasFromAnalysis(
    CanvasGraphAnalyzer& analyzer,
    const std::vector<DspModuleType>& pedalSlots,
    const std::vector<uint8_t>& manualRouting,
    const std::vector<ParameterDescriptor>& existingParams)
{
    PedalAssetPayload result;
    std::vector<DspModuleType> activeChain;
    std::vector<uint8_t> activeSlotIndices;

    for (int i = 0; i < static_cast<int>(pedalSlots.size()); ++i)
        if (pedalSlots[static_cast<size_t>(i)] != DspModuleType::BYPASS)
        {
            activeChain.push_back(pedalSlots[static_cast<size_t>(i)]);
            activeSlotIndices.push_back(static_cast<uint8_t>(i));
        }

    if (activeChain.empty())
        return result;

    std::vector<DspModuleType> sortedChain;
    std::vector<uint8_t> routingSlotOrder;
    if (!manualRouting.empty())
    {
        for (uint8_t slotIdx : manualRouting)
            if (slotIdx < pedalSlots.size() && pedalSlots[slotIdx] != DspModuleType::BYPASS)
            {
                sortedChain.push_back(pedalSlots[slotIdx]);
                routingSlotOrder.push_back(slotIdx);
            }
    }

    if (routingSlotOrder.empty())
    {
        const int activeCount = static_cast<int>(activeChain.size());
        const auto ranges = calculateRowRanges(activeCount);
        std::vector<int> scores(activeCount, 0);
        for (int i = 0; i < activeCount; ++i)
            scores[i] = analyzer.routingScore(ranges[static_cast<size_t>(i)]);

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

    const int activeCount = static_cast<int>(sortedChain.size());
    const auto sortedRanges = calculateRowRanges(activeCount);
    std::vector<ParameterDescriptor> parameters;
    for (int i = 0; i < activeCount; ++i)
    {
        const auto& definition = PedalDefinitions::get(sortedChain[static_cast<size_t>(i)]);
        for (const auto& parameter : definition.parameters)
        {
            ParameterDescriptor descriptor;
            descriptor.parameterToken = parameter.param.parameterToken;
            descriptor.minValue = parameter.param.minValue;
            descriptor.maxValue = parameter.param.maxValue;
            descriptor.defaultValue = parameter.param.defaultValue;
            descriptor.currentValue = parameter.param.defaultValue;
            descriptor.targetDspNodeRegister = static_cast<uint8_t>(i);
            descriptor.physicalSlot = routingSlotOrder[static_cast<size_t>(i)];
            parameters.push_back(descriptor);
        }
    }

    result.activeRoutingChain = sortedChain;
    result.routingSlotOrder = routingSlotOrder;
    for (const auto& parameter : parameters)
    {
        auto compiled = parameter;
        const int chainPos = static_cast<int>(parameter.targetDspNodeRegister);
        bool overridden = false;

        for (const auto& existing : existingParams)
            if (existing.physicalSlot == parameter.physicalSlot
                && existing.parameterToken == parameter.parameterToken
                && existing.isManualOverride)
            {
                compiled.currentValue = existing.currentValue;
                compiled.isManualOverride = true;
                overridden = true;
                break;
            }

        if (!overridden && chainPos >= 0 && chainPos < activeCount)
        {
            const auto& definition = PedalDefinitions::get(sortedChain[static_cast<size_t>(chainPos)]);
            const int parameterCount = static_cast<int>(definition.parameters.size());
            int parameterIndex = parameterCount;
            for (int i = 0; i < parameterCount; ++i)
                if (definition.parameters[static_cast<size_t>(i)].param.parameterToken == parameter.parameterToken)
                {
                    parameterIndex = i;
                    break;
                }

            if (parameterIndex < parameterCount)
            {
                const auto& definitionParameter = definition.parameters[static_cast<size_t>(parameterIndex)];
                const bool isMix = definitionParameter.label != nullptr
                                && std::strcmp(definitionParameter.label, "Mix") == 0;
                if (isMix)
                    compiled.currentValue = 1.0f;
                else
                {
                    float normalized = analyzer.pixelAccumulation(
                        sortedRanges[static_cast<size_t>(chainPos)], parameterIndex, parameterCount);
                    normalized = 0.5f + (normalized - 0.5f) * 0.5f;
                    compiled.currentValue = parameter.minValue
                        + normalized * (parameter.maxValue - parameter.minValue);
                }
            }
        }
        result.parameters.push_back(compiled);
    }

    return result;
}
}

PedalAssetPayload compileCanvas(const std::array<uint8_t, TotalCells>& gridData,
                                const std::vector<DspModuleType>& pedalSlots,
                                const std::vector<uint8_t>& manualRouting,
                                const std::vector<ParameterDescriptor>& existingParams)
{
    auto analyzer = std::make_unique<CanvasGraphAnalyzer>();
    DirtyRowMask allRows;
    allRows.fill(~uint64_t{ 0 });
    analyzer->update(gridData, allRows, 0);
    return compileCanvasFromAnalysis(*analyzer, pedalSlots, manualRouting, existingParams);
}

PedalAssetPayload compileCanvas(CanvasGraphAnalyzer& analyzer,
                                const std::array<uint8_t, TotalCells>& gridData,
                                const DirtyRowMask& dirtyRows,
                                uint32_t revision,
                                const std::vector<DspModuleType>& pedalSlots,
                                const std::vector<uint8_t>& manualRouting,
                                const std::vector<ParameterDescriptor>& existingParams)
{
    analyzer.update(gridData, dirtyRows, revision);
    return compileCanvasFromAnalysis(analyzer, pedalSlots, manualRouting, existingParams);
}
