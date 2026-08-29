#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "State/PedalDefinition.h"
#include "Effects/MiscEffects.h"

TEST_CASE("PedalDefinitions::snapValue snaps to the even grid", "[snap]")
{
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::RETIME, 0) == 0);
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::RETIME, 1) == 5);
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::RETIME, 2) == 4);
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::RETIME, 3) == 0);
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::SIDECHAIN, 0) == 5);
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::BITCRUSHER, 2) == 15);
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::CONVOLUTION_REVERB, 3) == 15);
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::TREMOLO, 3) == 3);

    REQUIRE(PedalDefinitions::snapValue(DspModuleType::DELAY, 1, 0.37f) == 0.37f);

    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 1, 0.0f) == 0.0f);
    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 1, 0.1f) == 0.0f);
    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 1, 0.13f) == 0.25f);
    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 1, 0.5f) == 0.5f);
    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 1, 0.99f) == 1.0f);
    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 1, 1.1f) == 1.0f);

    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 2, 0.33f) == 1.0f / 3.0f);
    REQUIRE(PedalDefinitions::snapValue(DspModuleType::RETIME, 2, 0.6f) == 2.0f / 3.0f);

    REQUIRE(PedalDefinitions::snapValue(DspModuleType::SIDECHAIN, 0, 0.12f) == 0.0f);
    REQUIRE(PedalDefinitions::snapValue(DspModuleType::SIDECHAIN, 0, 0.88f) == 1.0f);
}

TEST_CASE("Re-Time detents map to distinct ratios and bars", "[snap]")
{
    const float timeDetents[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (int i = 0; i < 5; ++i)
    {
        const float t = PedalDefinitions::snapValue(DspModuleType::RETIME, 1, timeDetents[i]);
        const int idx = static_cast<int>(std::lround(t * 4.0f));
        REQUIRE(idx == i);
    }

    const float barsDetents[4] = {0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f};
    for (int i = 0; i < 4; ++i)
    {
        const float t = PedalDefinitions::snapValue(DspModuleType::RETIME, 2, barsDetents[i]);
        const int idx = static_cast<int>(std::lround(t * 3.0f));
        REQUIRE(idx == i);
    }
}

TEST_CASE("Snap grids map to exact DSP quantizations", "[snap]")
{
    for (int i = 0; i < 15; ++i)
    {
        const float detent = static_cast<float>(i) / 14.0f;
        const float snapped = PedalDefinitions::snapValue(DspModuleType::BITCRUSHER, 2, detent);
        const float bits = 2.0f + snapped * 14.0f;
        REQUIRE(std::abs(bits - std::round(bits)) < 1e-4f);
        REQUIRE(bits >= 2.0f);
        REQUIRE(bits <= 16.0f);
    }

    int prevRow = -1;
    for (int i = 0; i < 15; ++i)
    {
        const float detent = static_cast<float>(i) / 14.0f;
        const float snapped = PedalDefinitions::snapValue(DspModuleType::CONVOLUTION_REVERB, 3, detent);
        const int row = static_cast<int>(std::round(snapped * 15.0f));
        REQUIRE(row > prevRow);
        REQUIRE(row <= 15);
        prevRow = row;
    }

    for (int i = 0; i < 3; ++i)
    {
        const float detent = static_cast<float>(i) * 0.5f;
        const float snapped = PedalDefinitions::snapValue(DspModuleType::TREMOLO, 3, detent);
        REQUIRE(snapped == detent);
    }
}

TEST_CASE("Pitch Shifter detents snap to integer semitones", "[snap]")
{
    REQUIRE(PedalDefinitions::snapSteps(DspModuleType::PITCH_SHIFTER, 2) == 25);
    for (int i = 0; i < 25; ++i)
    {
        const float detent = static_cast<float>(i) / 24.0f;
        const float snapped = PedalDefinitions::snapValue(DspModuleType::PITCH_SHIFTER, 2, detent);
        const float semitones = snapped * 24.0f - 12.0f;
        REQUIRE(std::abs(semitones - std::round(semitones)) < 1e-4f);
        REQUIRE(semitones >= -12.0f);
        REQUIRE(semitones <= 12.0f);
    }
}

TEST_CASE("Sidechain rate detents gate at the expected beat divisions", "[sidechain]")
{
    const int sr = 44100, ch = 1;
    const float detents[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    const float divisions[5] = {1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.5f, 1.0f};

    for (int i = 0; i < 5; ++i)
    {
        const int n = sr * 3;
        std::vector<float> storage(static_cast<size_t>(n), 1.0f);
        std::vector<float*> buf{ storage.data() };

        SidechainEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        eff.setTransport(120.0f, 0.0, true);
        float params[4] = { detents[i], 1.0f, 1.0f, 0.0f };
        eff.processBlock(buf.data(), ch, n, params);

        bool finite = true;
        int crossings = 0;
        for (int s = 1; s < n; ++s)
        {
            if (!std::isfinite(buf[0][s]))
                finite = false;
            if (buf[0][s - 1] < 0.5f && buf[0][s] >= 0.5f)
                ++crossings;
        }
        const float cyclesPerSecond = 120.0f / 60.0f / divisions[i];
        const float expected = 3.0f * cyclesPerSecond;
        REQUIRE(finite);
        REQUIRE(crossings > expected * 0.6f);
        REQUIRE(crossings < expected * 1.5f);
    }
}
