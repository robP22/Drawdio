#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

#include "Compile/CompilerEngine.h"
#include "Effects/DspEffect.h"

TEST_CASE("Incremental canvas analysis matches a full analysis", "[compiler]")
{
    std::array<uint8_t, TotalCells> initial{};
    for (int x = 8; x < 64; ++x)
        initial[static_cast<size_t>(32 * GridSize + x)] = 3;
    for (int x = 96; x < 160; ++x)
        initial[static_cast<size_t>(140 * GridSize + x)] = 7;

    const std::vector<DspModuleType> slots = {
        DspModuleType::TREMOLO,
        DspModuleType::WAVESHAPER,
        DspModuleType::DELAY,
        DspModuleType::BYPASS,
        DspModuleType::BYPASS,
        DspModuleType::BYPASS
    };

    auto analyzer = std::make_unique<CanvasGraphAnalyzer>();
    DirtyRowMask allRows;
    allRows.fill(~uint64_t{ 0 });
    auto first = compileCanvas(*analyzer, initial, allRows, 1, slots);

    auto changed = initial;
    for (int x = 180; x < 236; ++x)
        changed[static_cast<size_t>(32 * GridSize + x)] = 12;

    DirtyRowMask dirtyRows{};
    dirtyRows[0] = uint64_t{ 1 } << 32;
    auto incremental = compileCanvas(*analyzer, changed, dirtyRows, 2, slots);
    auto full = compileCanvas(changed, slots);

    REQUIRE(first.activeRoutingChain.size() == incremental.activeRoutingChain.size());
    REQUIRE(incremental.activeRoutingChain == full.activeRoutingChain);
    REQUIRE(incremental.routingSlotOrder == full.routingSlotOrder);
    REQUIRE(incremental.parameters.size() == full.parameters.size());
    for (size_t i = 0; i < full.parameters.size(); ++i)
        REQUIRE(std::abs(incremental.parameters[i].currentValue
                         - full.parameters[i].currentValue) < 1.0e-6f);
}
