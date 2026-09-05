#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <thread>

#include "State/ConfigManager.h"
#include "UnifiedPedalProcessor.h"

namespace
{
bool waitForCompiledResult(ConfigManager& config)
{
    for (int i = 0; i < 1000; ++i)
    {
        if (config.consumeCompiledResultIfAvailable())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
}

TEST_CASE("Same-topology canvas changes update parameters without replacing payload", "[compiler]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager config(dsp);
    config.prepare(44100.0, 512);

    config.setPedalSlot(0, DspModuleType::TREMOLO);
    REQUIRE(waitForCompiledResult(config));
    const auto* original = config.getCurrentConfig();
    REQUIRE(original != nullptr);

    auto grid = std::make_unique<std::array<uint8_t, TotalCells>>();
    grid->fill(7);
    config.submitCanvasSnapshot(*grid);
    REQUIRE(waitForCompiledResult(config));

    REQUIRE(config.getCurrentConfig() == original);
    REQUIRE(config.getLastConfigSync().parameters.size() == original->parameters.size());
}
