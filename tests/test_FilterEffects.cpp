#include <catch2/catch_test_macros.hpp>
#include "Effects/FilterEffects.h"

#include <vector>
#include <cmath>
#include <algorithm>

static void fillSine(float** buf, int ch, int n, float freq, double sr)
{
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
            buf[c][s] = 0.5f * std::sin(2.0f * 3.14159265f * freq
                          * static_cast<float>(s) / static_cast<float>(sr));
}

TEST_CASE("MultiModeFilter produces finite, non-silent output", "[filter]")
{
    MultiModeFilterEffect eff;
    const int sr = 44100, ch = 2, n = 1024;
    std::vector<std::vector<float>> storage(ch, std::vector<float>(n, 0.0f));
    std::vector<float*> buf(ch);
    for (int c = 0; c < ch; ++c) buf[c] = storage[c].data();

    fillSine(buf.data(), ch, n, 1000.0f, sr);
    eff.prepare(sr, ch);
    eff.reset();

    float params[4] = { 0.0f, 0.0f, 0.5f, 0.0f }; // mode 0 (LP), cutoff mid
    eff.processBlock(buf.data(), ch, n, params);

    bool finite = true, nonsilent = false;
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
        {
            if (!std::isfinite(buf[c][s])) finite = false;
            if (std::abs(buf[c][s]) > 1e-4f) nonsilent = true;
        }
    REQUIRE(finite);
    REQUIRE(nonsilent);
}

TEST_CASE("MultiModeFilter cutoff parameter has an effect", "[filter]")
{
    auto run = [&](float cutoffParam) -> std::vector<float>
    {
        MultiModeFilterEffect eff;
        const int sr = 44100, ch = 1, n = 2048;
        std::vector<float> storage(n, 0.0f);
        std::vector<float*> buf{ storage.data() };
        fillSine(buf.data(), ch, n, 2000.0f, sr);
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.0f, cutoffParam, 0.0f };
        eff.processBlock(buf.data(), ch, n, params);
        return storage;
    };

    auto low = run(0.05f);
    auto high = run(0.95f);

    float diff = 0.0f;
    for (size_t i = 0; i < low.size(); ++i)
        diff += std::abs(low[i] - high[i]);
    REQUIRE(diff > 1e-3f);
}

TEST_CASE("MultiModeFilter mode (Q/band) parameter has an effect", "[filter]")
{
    auto run = [&](float modeParam) -> std::vector<float>
    {
        MultiModeFilterEffect eff;
        const int sr = 44100, ch = 1, n = 2048;
        std::vector<float> storage(n, 0.0f);
        std::vector<float*> buf{ storage.data() };
        fillSine(buf.data(), ch, n, 2000.0f, sr);
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { modeParam, 0.0f, 0.5f, 0.0f };
        eff.processBlock(buf.data(), ch, n, params);
        return storage;
    };

    auto lp = run(0.0f);
    auto hp = run(0.99f);

    float diff = 0.0f;
    for (size_t i = 0; i < lp.size(); ++i)
        diff += std::abs(lp[i] - hp[i]);
    REQUIRE(diff > 1e-3f);
}

TEST_CASE("MultiModeFilter stays finite and stable at extreme cutoff", "[filter]")
{
    MultiModeFilterEffect eff;
    const int sr = 44100, ch = 1, n = 4096;
    std::vector<float> storage(n, 0.0f);
    std::vector<float*> buf{ storage.data() };
    fillSine(buf.data(), ch, n, 15000.0f, sr);
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.99f, 0.0f, 1.0f, 0.0f };
    eff.processBlock(buf.data(), ch, n, params);

    bool finite = true;
    for (int s = 0; s < n; ++s)
        if (!std::isfinite(buf[0][s]))
            finite = false;
    REQUIRE(finite);
}
