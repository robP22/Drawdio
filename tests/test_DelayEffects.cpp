#include <catch2/catch_test_macros.hpp>
#include "Effects/DelayEffects.h"

#include <vector>
#include <cmath>
#include <algorithm>

TEST_CASE("SimpleDelay produces delayed, non-silent, finite output", "[delay]")
{
    DelayEffect eff;
    const int sr = 44100, ch = 1, n = 8820; // 0.2s
    std::vector<float> storage(n, 0.0f);
    storage[0] = 1.0f; // impulse at t=0
    std::vector<float*> buf{ storage.data() };

    eff.prepare(sr, ch);
    eff.reset();

    float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // delay 0.1s, fb 0.3, damp 0
    eff.processBlock(buf.data(), ch, n, params);

    // First sample should be silent because the input is delayed.
    REQUIRE(std::abs(buf[0][0]) < 1e-6f);

    bool nonsilent = false, finite = true;
    for (int s = 1; s < n; ++s)
    {
        if (!std::isfinite(buf[0][s])) finite = false;
        if (std::abs(buf[0][s]) > 1e-4f) nonsilent = true;
    }
    REQUIRE(finite);
    REQUIRE(nonsilent);

    // The impulse should re-emerge near the 0.1s delay point (~4410 samples).
    float peakAroundDelay = 0.0f;
    for (int s = 4300; s < 4520; ++s)
        peakAroundDelay = std::max(peakAroundDelay, std::abs(buf[0][s]));
    REQUIRE(peakAroundDelay > 0.1f);
}

TEST_CASE("SimpleDelay feedback parameter increases the tail energy", "[delay]")
{
    auto run = [&](float feedbackParam) -> float
    {
        DelayEffect eff;
        const int sr = 44100, ch = 1, n = 44100; // 1.0s
        std::vector<float> storage(n, 0.0f);
        storage[0] = 1.0f;
        std::vector<float*> buf{ storage.data() };
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.0f, feedbackParam, 0.0f };
        eff.processBlock(buf.data(), ch, n, params);
        float energy = 0.0f;
        for (int s = 0; s < n; ++s) energy += buf[0][s] * buf[0][s];
        return energy;
    };

    float lowFb = run(0.0f);
    float highFb = run(0.95f);
    REQUIRE(highFb > lowFb);
}

TEST_CASE("SimpleDelay starts at the compiled delay time (no load glide)", "[delay]")
{
    DelayEffect eff;
    const int sr = 44100, ch = 1, n = 30000;
    std::vector<float> storage(n, 0.0f);
    storage[0] = 1.0f; // impulse at t=0
    std::vector<float*> buf{ storage.data() };

    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.5f, 0.0f, 0.0f }; // time 0.55s (default knob)
    eff.processBlock(buf.data(), ch, n, params);

    float peakEarly = 0.0f, peakAtDelay = 0.0f;
    for (int s = 0; s < 4000; ++s)
        peakEarly = std::max(peakEarly, std::abs(buf[0][s]));
    for (int s = 24000; s < 24600; ++s)
        peakAtDelay = std::max(peakAtDelay, std::abs(buf[0][s]));
    REQUIRE(peakEarly < 0.05f);
    REQUIRE(peakAtDelay > 0.1f);
}
