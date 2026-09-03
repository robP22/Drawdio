#include <catch2/catch_test_macros.hpp>
#include "Effects/DistortionEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/ModulationEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/ReTimeEffect.h"
#include "Effects/ReverseEffect.h"
#include "Effects/HpLpFilterEffect.h"
#include "Effects/FilterEffects.h"
#include "Effects/BitcrusherEffect.h"
#include "Effects/ConvolutionReverbEffect.h"
#include "Effects/SpectralFilterEffect.h"
#include "Effects/MiscEffects.h"
#include "Effects/GrainScrubberEffect.h"
#include "Dsp/GranularProcessor.h"

#include <vector>
#include <cmath>
#include <algorithm>

static float** makeBuffer(std::vector<std::vector<float>>& storage, int ch, int n)
{
    storage.assign(ch, std::vector<float>(n, 0.0f));
    auto* buf = new float*[ch];
    for (int c = 0; c < ch; ++c) buf[c] = storage[c].data();
    return buf;
}

static void fillSine(float** buf, int ch, int n, float freq, double sr, float amp = 0.8f)
{
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
            buf[c][s] = amp * std::sin(2.0f * 3.14159265f * freq
                          * static_cast<float>(s) / static_cast<float>(sr));
}

static bool allFinite(float** buf, int ch, int n)
{
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
            if (!std::isfinite(buf[c][s])) return false;
    return true;
}

static float peakAbs(float** buf, int ch, int n)
{
    float p = 0.0f;
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
            p = std::max(p, std::abs(buf[c][s]));
    return p;
}

static float sumAbsDiff(float** a, float** b, int ch, int n)
{
    float d = 0.0f;
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
            d += std::abs(a[c][s] - b[c][s]);
    return d;
}

// ---------------------------------------------------------------------------
TEST_CASE("Waveshaper produces non-silent finite output and drive has an effect", "[distortion]")
{
    const int sr = 44100, ch = 2, n = 2048;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 200.0f, sr);
    fillSine(bufB, ch, n, 200.0f, sr);

    WaveshaperEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pHigh[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("Wavefolder produces non-silent finite output and depth has an effect", "[distortion]")
{
    const int sr = 44100, ch = 1, n = 2048;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 300.0f, sr);
    fillSine(bufB, ch, n, 300.0f, sr);

    WavefolderEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };   // Mix 0, Shift 0
    float pHigh[4] = { 0.0f, 1.0f, 0.0f, 0.0f };   // Mix 0, Shift 2 kHz
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("Wavefolder quiet-signal noise floor", "[distortion]")
{
    // The old secant fold (F2-F1)/dx subtracted two ~-1/(a*norm) constants, so
    // tiny input steps amplified float roundoff into crackle pops. The output
    // must match the ideal pointwise fold sin(a*x)/norm on quiet material.
    const int sr = 44100, ch = 1;
    const float drives[2] = { 0.0f, 1.0f };
    for (int di = 0; di < 2; ++di)
    {
        const int n = 44100;
        std::vector<std::vector<float>> storage;
        float** buf = makeBuffer(storage, ch, n);
        for (int s = 0; s < n; ++s)
            buf[0][s] = 1.0e-4f * std::sin(2.0f * 3.14159265f * 440.0f
                        * static_cast<float>(s) / static_cast<float>(sr));

        WavefolderEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, drives[di], 0.0f, 0.0f };
        eff.processBlock(buf, ch, n, params);

        const float d = 1.0f + drives[di] * 4.0f;
        const float norm = 1.0f + d * 0.1f;
        const float a = d * 3.14159265f;
        float maxDev = 0.0f;
        for (int s = 0; s < n; ++s)
        {
            const float input = 1.0e-4f * std::sin(2.0f * 3.14159265f * 440.0f
                                * static_cast<float>(s) / static_cast<float>(sr));
            const float ideal = std::sin(a * input) / norm;
            const float dev = std::abs(buf[0][s] - ideal);
            maxDev = std::max(maxDev, dev);
        }
        REQUIRE(allFinite(buf, ch, n));
        REQUIRE(maxDev < 1.0e-3f);
        delete[] buf;
    }

    // Worst case: near-DC input with per-sample steps of 1e-7.
    {
        const int n = 44100;
        std::vector<std::vector<float>> storage;
        float** buf = makeBuffer(storage, ch, n);
        for (int s = 0; s < n; ++s)
            buf[0][s] = 1.0e-7f * static_cast<float>(s);

        WavefolderEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        eff.processBlock(buf, ch, n, params);

        const float d = 1.0f;
        const float norm = 1.0f + d * 0.1f;
        const float a = d * 3.14159265f;
        float maxDev = 0.0f;
        for (int s = 0; s < n; ++s)
        {
            const float input = 1.0e-7f * static_cast<float>(s);
            const float ideal = std::sin(a * input) / norm;
            const float dev = std::abs(buf[0][s] - ideal);
            maxDev = std::max(maxDev, dev);
        }
        REQUIRE(allFinite(buf, ch, n));
        REQUIRE(maxDev < 1.0e-3f);
        delete[] buf;
    }
}

TEST_CASE("CombResonator produces non-silent finite output and freq has an effect", "[distortion]")
{
    // Comb delay at low freq (~20Hz) is ~2205 samples, so use a block longer
    // than that to actually capture the delayed (non-silent) output.
    const int sr = 44100, ch = 1, n = 8192;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 500.0f, sr);
    fillSine(bufB, ch, n, 500.0f, sr);

    CombResonatorEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pHigh[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

// ---------------------------------------------------------------------------
TEST_CASE("DiffusedReverb produces non-silent finite output with a tail; decay has an effect", "[reverb]")
{
    const int sr = 44100, ch = 2, n = 8192;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    // Impulse train so the network has something to diffuse.
    for (int s = 0; s < n; s += 1024) bufA[0][s] = bufA[1][s] = 1.0f;
    for (int s = 0; s < n; s += 1024) bufB[0][s] = bufB[1][s] = 1.0f;

    DiffusedReverbEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };   // Mix 0, Shift 0
    float pHigh[4] = { 0.0f, 1.0f, 0.0f, 0.0f };   // Mix 0, Shift 2 kHz
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(allFinite(bufB, ch, n));
    REQUIRE(peakAbs(bufB, ch, n) > 1e-4f);
    REQUIRE(eff.hasActiveTail());
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("PlateReverb produces non-silent finite output", "[reverb]")
{
    const int sr = 44100, ch = 2, n = 8192;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; s += 1024) buf[0][s] = buf[1][s] = 1.0f;

    PlateReverbEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.5f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(peakAbs(buf, ch, n) > 1e-4f);
    delete[] buf;
}

TEST_CASE("ConvolutionReverb produces a wet tail past the input burst", "[reverb]")
{
    const int sr = 44100, ch = 1, n = sr;  // 1 s
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < 200; ++s)
        buf[0][s] = 0.8f * std::sin(6.2831853f * 440.0f * s / sr);
    for (int s = 200; s < n; ++s)
        buf[0][s] = 0.0f;

    ConvolutionReverbEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.5f, 0.5f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(peakAbs(buf, ch, n) > 0.1f);
    // The reverb tail must continue well past the 200-sample burst.
    float lateEnergy = 0.0f;
    for (int s = static_cast<int>(sr * 0.3f); s < static_cast<int>(sr * 0.9f); ++s)
        lateEnergy += std::abs(buf[0][s]);
    REQUIRE(lateEnergy > 1.0e-3f);
    delete[] buf;
}

TEST_CASE("ConvolutionReverb Size knob changes the decay length", "[reverb]")
{
    const int sr = 44100, ch = 1, n = sr;
    std::vector<std::vector<float>> sShort, sLong;
    float** bufShort = makeBuffer(sShort, ch, n);
    float** bufLong = makeBuffer(sLong, ch, n);
    for (int s = 0; s < 200; ++s)
    {
        const float v = 0.8f * std::sin(6.2831853f * 440.0f * s / sr);
        bufShort[0][s] = v;
        bufLong[0][s] = v;
    }
    for (int s = 200; s < n; ++s)
    {
        bufShort[0][s] = 0.0f;
        bufLong[0][s] = 0.0f;
    }

    const auto lateEnergy = [](float** b, int totalN, int srIn) {
        float e = 0.0f;
        for (int s = static_cast<int>(srIn * 0.6f); s < static_cast<int>(srIn * 0.9f); ++s)
            e += std::abs(b[0][s]);
        return e;
    };
    {
        ConvolutionReverbEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.0f, 0.5f, 0.5f };   // RT60 0.15 s
        eff.processBlock(bufShort, ch, n, params);
    }
    {
        ConvolutionReverbEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 1.0f, 0.5f, 0.5f };   // RT60 1.5 s
        eff.processBlock(bufLong, ch, n, params);
    }

    REQUIRE(allFinite(bufShort, ch, n));
    REQUIRE(allFinite(bufLong, ch, n));
    const float eShort = lateEnergy(bufShort, n, sr);
    const float eLong = lateEnergy(bufLong, n, sr);
    REQUIRE(eLong > eShort * 3.0f);
    delete[] bufShort; delete[] bufLong;
}

TEST_CASE("ConvolutionReverb Damp knob changes the output", "[reverb]")
{
    const int sr = 44100, ch = 1, n = static_cast<int>(sr * 0.5f);
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    float paramsA[4] = { 0.0f, 0.5f, 0.5f, 0.0f };
    float paramsB[4] = { 0.0f, 0.5f, 0.5f, 1.0f };
    {
        ConvolutionReverbEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        eff.processBlock(bufA, ch, n, paramsA);
    }
    {
        ConvolutionReverbEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        eff.processBlock(bufB, ch, n, paramsB);
    }

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(allFinite(bufB, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 0.1f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 0.1f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("ConvolutionReverb Width knob decorrelates the stereo tails", "[reverb]")
{
    const int sr = 44100, ch = 2, n = static_cast<int>(sr * 0.5f);
    std::vector<std::vector<float>> sW0, sW1;
    float** bufW0 = makeBuffer(sW0, ch, n);
    float** bufW1 = makeBuffer(sW1, ch, n);
    for (int s = 0; s < n; ++s)
    {
        const float v = (s < 200) ? 0.8f * std::sin(6.2831853f * 440.0f * s / sr) : 0.0f;
        bufW0[0][s] = v;
        bufW0[1][s] = v;
        bufW1[0][s] = v;
        bufW1[1][s] = v;
    }

    const auto lrDiff = [](float** b, int totalN) {
        float d = 0.0f;
        for (int s = 0; s < totalN; ++s)
            d += std::abs(b[0][s] - b[1][s]);
        return d;
    };
    {
        ConvolutionReverbEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.5f, 0.0f, 0.5f };
        eff.processBlock(bufW0, ch, n, params);
    }
    {
        ConvolutionReverbEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.5f, 1.0f, 0.5f };
        eff.processBlock(bufW1, ch, n, params);
    }

    REQUIRE(allFinite(bufW0, ch, n));
    REQUIRE(allFinite(bufW1, ch, n));
    const float dW0 = lrDiff(bufW0, n);
    const float dW1 = lrDiff(bufW1, n);
    REQUIRE(dW0 < 1.0e-4f);          // width 0: identical tails (correlated spectra)
    REQUIRE(dW1 > dW0 * 10.0f);
    delete[] bufW0; delete[] bufW1;
}

// ---------------------------------------------------------------------------
TEST_CASE("Tremolo produces non-silent output and depth has an effect", "[modulation]")
{
    const int sr = 44100, ch = 1, n = 4096;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    TremoloEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.5f, 0.0f, 0.0f };
    float pHigh[4] = { 0.0f, 0.5f, 1.0f, 0.0f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("Flanger produces non-silent output and feedback has an effect", "[modulation]")
{
    const int sr = 44100, ch = 1, n = 4096;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    FlangerEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.5f, 0.5f, 0.0f };
    float pHigh[4] = { 0.0f, 0.5f, 0.5f, 0.9f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

// ---------------------------------------------------------------------------
TEST_CASE("GranularPitch produces non-silent finite output and rate has an effect", "[pitch]")
{
    // Granular effects read back from a delay buffer, so run past the initial
    // grain (a few thousand samples) before asserting non-silence.
    const int sr = 44100, ch = 1, n = 30000;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    PitchShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pHigh[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("Pitch Shifter is continuous at unity pitch", "[pitch]")
{
    const int sr = 44100, ch = 1, n = 88200;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; ++s)
        buf[0][s] = static_cast<float>(s) * 1.0e-4f;

    PitchShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.5f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    // Unity pitch must read the source continuously: per-sample deltas stay at
    // the ramp slope (1e-4) instead of jumping at grain boundaries.
    float maxDelta = 0.0f;
    for (int s = 1; s < n; ++s)
        maxDelta = std::max(maxDelta, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(maxDelta < 2.0e-4f);

    // The output is a delayed copy of the ramp with a fixed ~0.3s latency
    // (compare only after the 1s delay line has fully recorded).
    float bestErr = 1.0e30f;
    int bestLag = -1;
    for (int lag = 12500; lag < 14000; ++lag)
    {
        float err = 0.0f;
        for (int s = 40000; s < n; ++s)
            err += std::abs(buf[0][s] - static_cast<float>(s - lag) * 1.0e-4f);
        if (err < bestErr) { bestErr = err; bestLag = lag; }
    }
    REQUIRE(bestLag >= 13000);
    REQUIRE(bestLag <= 13500);
    float meanErr = 0.0f;
    for (int s = 40000; s < n; ++s)
        meanErr += std::abs(buf[0][s] - static_cast<float>(s - bestLag) * 1.0e-4f);
    REQUIRE(meanErr / static_cast<float>(n - 40000) < 1.0e-6f);
    delete[] buf;
}

TEST_CASE("Pitch Shifter shifts pitch by the expected ratio", "[pitch]")
{
    const int sr = 44100, ch = 1, n = 88200;
    const int windowStart = n - 35280;
    const float pitches[3] = { 0.0f, 0.5f, 1.0f };
    const float expected[3] = { 220.0f, 440.0f, 880.0f };

    for (int i = 0; i < 3; ++i)
    {
        std::vector<std::vector<float>> storage;
        float** buf = makeBuffer(storage, ch, n);
        fillSine(buf, ch, n, 440.0f, sr);

        PitchShifterEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.0f, pitches[i], 0.0f };
        eff.processBlock(buf, ch, n, params);

        int crossings = 0;
        bool up = buf[0][windowStart] >= 0.0f;
        for (int s = windowStart + 1; s < n; ++s)
        {
            const bool nowUp = buf[0][s] >= 0.0f;
            if (up != nowUp) ++crossings;
            up = nowUp;
        }
        const float freq = static_cast<float>(crossings) * static_cast<float>(sr)
                         * 0.5f / static_cast<float>(n - windowStart);
        REQUIRE(allFinite(buf, ch, n));
        REQUIRE(std::abs(freq - expected[i]) < 0.05f * expected[i]);
        delete[] buf;
    }
}

TEST_CASE("Pitch Shifter crossfade is click-free at 2x", "[pitch]")
{
    const int sr = 44100, ch = 1, n = 88200;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.8f);

    PitchShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    float maxDelta = 0.0f;
    for (int s = n - 44100 + 1; s < n; ++s)
        maxDelta = std::max(maxDelta, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(allFinite(buf, ch, n));
    // 880 Hz sine at 0.8 amp: slope <= 0.8*2*pi*880/44100 ~ 0.10. A hard splice
    // at the loop point would produce deltas well above this.
    REQUIRE(maxDelta < 0.30f);
    delete[] buf;
}

TEST_CASE("Pitch Shifter up-shift latency stays short", "[pitch]")
{
    // At 2x the read re-anchors via crossfades; the output latency cycles in
    // [1.5X, 2.5X] (X = the crossfade/tap length). A 120ms tap put notes
    // 180-300ms late ("some notes play shortly after they are supposed to").
    const int sr = 44100, ch = 1, n = 88200;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    buf[0][44100] = 1.0f; // single impulse mid-run

    PitchShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    int peakIdx = 44100;
    for (int s = 44100; s < n; ++s)
        if (std::abs(buf[0][s]) > std::abs(buf[0][peakIdx])) peakIdx = s;
    const float latencySec = static_cast<float>(peakIdx - 44100) / static_cast<float>(sr);
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(std::abs(buf[0][peakIdx]) > 0.3f);
    REQUIRE(latencySec < 0.15f);
    delete[] buf;
}

TEST_CASE("Pitch Shifter stays continuous at 0.5x past the lap point", "[pitch]")
{
    const int sr = 44100, ch = 1, n = sr * 3;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.8f);

    PitchShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    // Pre-fix, the read head lapped the write head ~1.4s in (gap reached the 1s
    // ring), jumping from ~1s-old material to near-live -> step up to ~1.6.
    float maxDelta = 0.0f;
    for (int s = sr + sr / 2 + 1; s < n; ++s)
        maxDelta = std::max(maxDelta, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(allFinite(buf, ch, n));
    // 220 Hz output slope ~0.025; window crossfades blend two 220 Hz reads.
    REQUIRE(maxDelta < 0.15f);
    delete[] buf;
}

TEST_CASE("Pitch Shifter halves the pitch at 0.5x with wrap crossfades", "[pitch]")
{
    const int sr = 44100, ch = 1, n = sr * 3;
    const int windowStart = sr + sr / 2;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr);

    PitchShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    int crossings = 0;
    bool up = buf[0][windowStart] >= 0.0f;
    for (int s = windowStart + 1; s < n; ++s)
    {
        const bool nowUp = buf[0][s] >= 0.0f;
        if (up != nowUp) ++crossings;
        up = nowUp;
    }
    const float freq = static_cast<float>(crossings) * static_cast<float>(sr)
                     * 0.5f / static_cast<float>(n - windowStart);
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(std::abs(freq - 220.0f) < 0.05f * 220.0f);
    delete[] buf;
}

TEST_CASE("FrequencyShifter produces non-silent output and shift has an effect", "[pitch]")
{
    const int sr = 44100, ch = 1, n = 4096;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    FrequencyShifterEffect eff;
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pHigh[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    eff.prepare(sr, ch);
    eff.reset();
    eff.processBlock(bufA, ch, n, pLow);

    FrequencyShifterEffect highEff;
    highEff.prepare(sr, ch);
    highEff.reset();
    highEff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("GlitchStutter produces non-silent finite output", "[pitch]")
{
    const int sr = 44100, ch = 1, n = 8192;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr);

    GlitchStutterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.5f, 0.0f, 0.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(peakAbs(buf, ch, n) > 1e-4f);
    delete[] buf;
}

TEST_CASE("ReTime recapture splices stay smooth on a ramp", "[retime]")
{
    const int sr = 44100, ch = 1, n = 88200;
    const float k = 1.0e-4f;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; ++s)
        buf[0][s] = k * static_cast<float>(s);

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.5f, 0.5f, 0.5f }; // 0.75x, 2 bars, shift 0.5
    eff.processBlock(buf, ch, n, params);

    // Loop wraps ~1.3s in (0.5L at 0.75x) and recaptures; the ramp is read at
    // 0.75x (slope 0.75k), and the 25ms fade's morph steps are ~k*0.5L*(pi/2/L).
    // A splice without a fade would step by ~k*0.5L = 2.2.
    float maxStep = 0.0f;
    for (int s = static_cast<int>(sr * 1.2); s < n; ++s)
        maxStep = std::max(maxStep, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(maxStep < 0.01f);
    delete[] buf;
}

TEST_CASE("Reverse slice splices stay smooth on a ramp", "[reverse]")
{
    const int sr = 44100, ch = 1, n = 88200;
    const float k = 1.0e-4f;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; ++s)
        buf[0][s] = k * static_cast<float>(s);

    ReverseEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.5f, 0.5f }; // smooth 0.5, density 0.5
    eff.processBlock(buf, ch, n, params);

    // Reversed playback slope is -k; a hard splice at a pass boundary would
    // step by ~k*sliceLen (~2.3 at density 0.5).
    float maxStep = 0.0f;
    for (int s = static_cast<int>(sr * 0.5); s < n; ++s)
        maxStep = std::max(maxStep, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(maxStep < 0.01f);
    delete[] buf;
}

TEST_CASE("Glitch Stutter splices stay smooth on a ramp", "[glitch]")
{
    const int sr = 44100, ch = 1, n = 88200;
    const float k = 1.0e-4f;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; ++s)
        buf[0][s] = k * static_cast<float>(s);

    GlitchStutterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.5f, 0.0f, 0.0f, 0.5f }; // intens 0.5, random 0, smooth 0.5
    eff.processBlock(buf, ch, n, params);

    // The slice slope is k; the gate fade-ins grow at ~v*pi/(2*xfadeLen) (v =
    // the ramp value, ~0.025 at t=2s). A splice without a fade would step by
    // ~k*sliceLen (~2.8 at intensity 0.5).
    float maxStep = 0.0f;
    for (int s = static_cast<int>(sr); s < n; ++s)
        maxStep = std::max(maxStep, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(maxStep < 0.05f);
    delete[] buf;
}

TEST_CASE("GlitchStutter repeats the frozen slice (no live contamination)", "[pitch]")
{
    const int sr = 44100, ch = 1;
    constexpr int kX = static_cast<int>(44100 * 0.015); // 15ms xfade floor at Smooth = 0
    const float intensity = 0.5f;                    // sliceLen = 12128, maxRepeats = 3
    const int sliceLen = static_cast<int>(sr * 0.275f + 0.5f);
    const int T0 = sliceLen;                         // first PLAYING sample
    const int n = T0 + sliceLen * 3 + 64;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr);

    GlitchStutterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { intensity, 0.0f, 0.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));

    auto sineVal = [&](int idx) {
        return 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * idx / sr);
    };

    // Pass 2 replays the frozen slice content (direct region, content index = playCounter2)
    for (int t = T0 + sliceLen; t < T0 + 2 * sliceLen - kX - kX; ++t)
        REQUIRE(std::abs(buf[0][t] - sineVal(t - T0 - sliceLen + kX)) < 1e-4f);

    // Loop crossfades blend the slice tail into the true slice head (frozen)
    for (int off = 0; off < kX; ++off)
    {
        const float fadeIn  = 0.5f * (1.0f - std::cos(3.14159265f * static_cast<float>(off) / kX));
        const float fadeOut = 0.5f * (1.0f + std::cos(3.14159265f * static_cast<float>(off) / kX));

        const int t1 = T0 + sliceLen - kX + off;                 // pass 1 xfade
        REQUIRE(std::abs(buf[0][t1] - (sineVal(sliceLen - kX + off) * fadeOut + sineVal(off) * fadeIn)) < 1e-4f);

        const int t2 = T0 + 2 * sliceLen - 2 * kX + off;         // pass 2 xfade (was live-contaminated pre-freeze)
        REQUIRE(std::abs(buf[0][t2] - (sineVal(sliceLen - kX + off) * fadeOut + sineVal(off) * fadeIn)) < 1e-4f);
    }
    delete[] buf;
}

TEST_CASE("GlitchStutter cycle rate scales with intensity (inverted mapping)", "[pitch]")
{
    const int sr = 44100, ch = 1, n = sr * 5;
    const int sliceHi = static_cast<int>(sr * 0.05f + 0.5f);
    const int cycleHi = sliceHi * 6 + sliceHi / 4;
    const int sliceLo = static_cast<int>(sr * 0.5f + 0.5f);
    const int cycleLo = sliceLo * 2 + sliceLo / 4;

    auto countGateEdges = [&](float intensity) -> int
    {
        std::vector<std::vector<float>> storage;
        float** buf = makeBuffer(storage, ch, n);
        for (int s = 0; s < n; ++s) buf[0][s] = 0.5f;
        GlitchStutterEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { intensity, 0.0f, 0.0f, 0.0f };
        eff.processBlock(buf, ch, n, params);
        int edges = 0;
        for (int s = 1; s < n; ++s)
            if (buf[0][s - 1] < 0.25f && buf[0][s] >= 0.25f)
                ++edges;
        delete[] buf;
        return edges;
    };

    const int edgesHi = countGateEdges(1.0f);
    const int edgesLo = countGateEdges(0.0f);
    REQUIRE(edgesHi > 2 * edgesLo);
    REQUIRE(edgesHi >= n / cycleHi - 1);
    REQUIRE(edgesLo <= n / cycleLo + 2);
}

// ---------------------------------------------------------------------------
TEST_CASE("ReTime produces non-silent finite output and speed has an effect", "[retime]")
{
    const int sr = 44100, ch = 1, n = 400000;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    ReTimeEffect effA, effB;
    effA.prepare(sr, ch); effA.reset();
    effB.prepare(sr, ch); effB.reset();
    float pLow[4]  = { 0.0f, 0.6f, 0.5f, 0.5f };
    float pHigh[4] = { 0.0f, 1.0f, 0.5f, 0.5f };
    effA.processBlock(bufA, ch, n, pLow);
    effB.processBlock(bufB, ch, n, pHigh);

    const int start = 360000;
    float peakA = 0.0f, diff = 0.0f;
    for (int s = start; s < n; ++s)
    {
        peakA = std::max(peakA, std::abs(bufA[0][s]));
        diff += std::abs(bufA[0][s] - bufB[0][s]);
    }

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakA > 1e-4f);
    REQUIRE(diff > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("ReTime passes audio while priming its first loop", "[retime]")
{
    const int sr = 1000, ch = 1, n = 5000;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; ++s)
        buf[0][s] = 0.5f;

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    eff.setTransport(60.0f, 0.0, true);
    float params[4] = { 0.0f, 0.75f, 0.3333333f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    float earlyPeak = 0.0f;
    for (int s = 0; s < sr / 2; ++s)
        earlyPeak = std::max(earlyPeak, std::abs(buf[0][s]));

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(earlyPeak > 0.49f);
    delete[] buf;
}

TEST_CASE("ReTime responds to transport", "[retime]")
{
    const int sr = 44100, ch = 1, n = 400000;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr);

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    eff.setTransport(120.0f, 0.0, true);
    float params[4] = { 0.0f, 0.5f, 0.5f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    float peak = 0.0f;
    for (int s = 360000; s < n; ++s) peak = std::max(peak, std::abs(buf[0][s]));
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(peak > 1e-4f);
    delete[] buf;
}

TEST_CASE("ReTime resynchronizes after a host playhead seek", "[retime]")
{
    const int sr = 1000, ch = 1, n = 6564;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; ++s)
        buf[0][s] = static_cast<float>(s) * 0.001f;

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    eff.setTransport(60.0f, 0.0, true);
    float params[4] = { 0.0f, 0.75f, 0.3333333f, 0.0f };
    eff.processBlock(buf, ch, n - 64, params);

    eff.setTransport(60.0f, 2.0, true);
    float* seekBuffer[1] = { storage[0].data() + n - 64 };
    eff.processBlock(seekBuffer, ch, 64, params);

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(std::abs(seekBuffer[0][63] - 0.56f) < 0.05f);
    delete[] buf;
}

TEST_CASE("ReTime passes full-band audio (no heavy lowpass)", "[retime]")
{
    const int sr = 44100, ch = 1, n = 400000;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.8f);

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.5f, 0.5f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    float peak = 0.0f;
    for (int s = 360000; s < n; ++s) peak = std::max(peak, std::abs(buf[0][s]));
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(peak > 0.5f);
    delete[] buf;
}

TEST_CASE("ReTime timing change is click-free", "[retime]")
{
    const int sr = 44100, ch = 1, n = 400000;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.8f);

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float paramsA[4] = { 0.0f, 0.5f, 0.5f, 0.5f };
    float paramsB[4] = { 0.0f, 1.0f, 0.5f, 0.5f };
    eff.processBlock(buf, ch, n / 2, paramsA);
    float* bufShift[1];
    bufShift[0] = storage[0].data() + n / 2;
    eff.processBlock(bufShift, ch, n / 2, paramsB);

    float maxDelta = 0.0f;
    int maxIdx = 0;
    for (int s = 1; s < n; ++s)
    {
        const float d = std::abs(buf[0][s] - buf[0][s - 1]);
        if (d > maxDelta) { maxDelta = d; maxIdx = s; }
    }
    INFO("maxDelta " << maxDelta << " at sample " << maxIdx);
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(maxDelta < 0.25f);
    delete[] buf;
}

TEST_CASE("ReTime fades to silence when transport stops", "[retime]")
{
    const int sr = 44100, ch = 1;
    const int phase1 = static_cast<int>(sr * 2.5);
    const int phase2 = static_cast<int>(sr * 1.0);
    const int n = phase1 + phase2;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    for (int s = 0; s < n; ++s)
        buf[0][s] = static_cast<float>(s) * 0.0001f;

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 1.0f, 0.3333333f, 0.0f };

    eff.setTransport(120.0f, 0.0, true);
    eff.processBlock(buf, ch, phase1, params);

    eff.setTransport(120.0f, 0.0, false);
    float* bufShift[1];
    bufShift[0] = storage[0].data() + phase1;
    eff.processBlock(bufShift, ch, phase2, params);

    float stoppedPeak = 0.0f;
    for (int s = phase1 + static_cast<int>(sr * 0.2); s < n; ++s)
        stoppedPeak = std::max(stoppedPeak, std::abs(buf[0][s]));

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(stoppedPeak < 1.0e-4f);
    REQUIRE_FALSE(eff.hasActiveTail());
    delete[] buf;
}

TEST_CASE("ReTime releases after silent input without transport", "[retime]")
{
    const int sr = 1000, ch = 1;
    const int warmup = 5000;
    const int silence = 1000;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, warmup + silence);
    for (int s = 0; s < warmup; ++s)
        buf[0][s] = 0.5f;

    ReTimeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.75f, 0.3333333f, 0.0f };
    eff.processBlock(buf, ch, warmup, params);

    float* silenceBuffer[1] = { storage[0].data() + warmup };
    eff.processBlock(silenceBuffer, ch, silence, params);

    float tailPeak = 0.0f;
    for (int s = static_cast<int>(sr * 0.6); s < silence; ++s)
        tailPeak = std::max(tailPeak, std::abs(silenceBuffer[0][s]));

    REQUIRE(allFinite(buf, ch, warmup + silence));
    REQUIRE(tailPeak < 1.0e-4f);
    REQUIRE_FALSE(eff.hasActiveTail());
    delete[] buf;
}

TEST_CASE("Spectral Freeze loop wrap stays click-free over many cycles", "[freeze]")
{
    const int sr = 44100, ch = 1;
    const int n = static_cast<int>(sr * 3.5);
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.8f);

    SpectralFreezeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.5f, 0.0f, 0.0f, 0.0f };   // freeze on, pitch 1.0, Offset 0
    eff.processBlock(buf, ch, n, params);

    // Wraps every ~0.89s past the 1s history warm-up; the advancing-head wrap
    // crossfade keeps per-sample steps near the signal's own slope.
    float maxDelta = 0.0f;
    for (int s = static_cast<int>(sr * 1.5); s < n; ++s)
        maxDelta = std::max(maxDelta, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(maxDelta < 0.15f);
    delete[] buf;
}

// ---------------------------------------------------------------------------
TEST_CASE("GranularProcessor restart is click-free (DC through dual grains)", "[granular]")
{
    const double sr = 44100.0;
    const float dc = 0.5f;
    GranularProcessorState st;
    prepareGranularProcessor(st, sr, 2.0);
    // Warm up past a full buffer cycle so every grain region holds recorded DC.
    for (int i = 0; i < 88200 * 3; ++i)
        processGranularSample(dc, st, 2.0f, sr, 0.08f, 0.5f);
    float prev = dc;
    float maxStep = 0.0f;
    for (int i = 0; i < 88200 * 2; ++i)
    {
        const float out = processGranularSample(dc, st, 2.0f, sr, 0.08f, 0.5f);
        maxStep = std::max(maxStep, std::abs(out - prev));
        prev = out;
    }
    // Unity-sum Hann windows keep DC at DC; a restart click would step up to ~0.5.
    REQUIRE(maxStep < 0.01f);
    REQUIRE(std::abs(prev - dc) < 0.01f);
}

TEST_CASE("GranularProcessor never reads at or past the write head", "[granular]")
{
    const double sr = 44100.0;
    const float positions[3] = { 0.0f, 0.5f, 0.99f };
    const float speeds[3] = { 0.5f, 1.0f, 2.0f };
    for (float pos : positions)
        for (float speed : speeds)
        {
            GranularProcessorState st;
            prepareGranularProcessor(st, sr, 2.0);
            const size_t bufSize = st.delayBuf.size();
            for (int i = 0; i < 88200 * 3; ++i)
            {
                processGranularSample(0.25f, st, speed, sr, 0.08f, pos);
                const size_t readA = (st.grainBaseA + static_cast<size_t>(st.readPtrA)) % bufSize;
                const size_t readB = (st.grainBaseB + static_cast<size_t>(st.readPtrB)) % bufSize;
                const size_t distA = (st.writePtr + bufSize - readA) % bufSize;
                const size_t distB = (st.writePtr + bufSize - readB) % bufSize;
                REQUIRE(distA >= 100);
                REQUIRE(distB >= 100);
            }
        }
}

TEST_CASE("Chorus produces non-silent output and depth has an effect", "[modulation]")
{
    const int sr = 44100, ch = 2, n = 4096;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    ChorusEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.0f, 0.0f, 0.5f, 0.5f };
    float pHigh[4] = { 0.0f, 1.0f, 0.5f, 0.5f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("Chorus delay stays bounded over a long soak (no tap drift)", "[modulation]")
{
    const int sr = 44100, ch = 1, n = sr * 60;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.5f);

    ChorusEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 1.0f, 0.5f, 1.0f };
    eff.processBlock(buf, ch, n, params);

    auto zeroCrossRate = [&](int start, int len) -> float
    {
        int count = 0;
        for (int s = start + 1; s < start + len; ++s)
            if ((buf[0][s - 1] < 0.0f) != (buf[0][s] < 0.0f))
                ++count;
        return static_cast<float>(count) / static_cast<float>(len) * static_cast<float>(sr);
    };

    const float zcrEarly = zeroCrossRate(44100 * 2, 44100 * 2);
    const float zcrLate = zeroCrossRate(44100 * 55, 44100 * 2);
    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(zcrEarly > 100.0f);
    REQUIRE(std::abs(zcrEarly - zcrLate) / zcrEarly < 0.02f);
    delete[] buf;
}

TEST_CASE("ReverseBuffer repeats are crossfaded (no hard splice)", "[reverse]")
{
    const int sr = 44100, ch = 1, n = sr * 8;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 220.0f, sr, 0.8f);

    ReverseEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.0f, 0.2f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    float maxStep = 0.0f;
    for (int s = 1; s < n; ++s)
        maxStep = std::max(maxStep, std::abs(buf[0][s] - buf[0][s - 1]));
    // 220 Hz sine at amp 0.8 steps ~0.05/sample; a hard splice jumps up to ~1.6.
    REQUIRE(maxStep < 0.15f);
    delete[] buf;
}

TEST_CASE("HP/LP Filter resonance stays bounded at maximum Reso", "[filter]")
{
    const int sr = 44100, ch = 1, n = 8820;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 1000.0f, sr, 0.5f);

    HpLpFilterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    // HP 855Hz, LP cutoff at 1kHz, Reso max (Q capped at 2.0).
    float params[4] = { 1.0f, 0.8f, 0.2f, 1.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    // Correct TDF-II cascade at the LP cutoff peaks ~3.9x (11.7 dB);
    // the old x-form implementation blew up to ~760x (DC-pole instability).
    REQUIRE(peakAbs(buf, ch, n) < 0.5f * 5.0f);
    delete[] buf;
}

TEST_CASE("HP/LP Filter is transparent at the default knob positions", "[filter]")
{
    const int sr = 44100, ch = 1, n = 8820;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 1000.0f, sr, 0.5f);

    HpLpFilterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 1.0f, 0.0f, 1.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    const float peak = peakAbs(buf, ch, n);
    REQUIRE(peak > 0.4f);
    REQUIRE(peak < 0.6f);
    delete[] buf;
}

TEST_CASE("SpectralFreeze defers until the ring has a full capture window", "[filter]")
{
    // A freshly placed pedal freezes immediately (canvas-compiled Freeze 0.3-0.7).
    // The capture window is the last 1s of the ring, so before 1s of audio the
    // freeze must pass the live signal instead of freezing an empty ring.
    const int sr = 44100, ch = 1, n = 66150; // 1.5s
    std::vector<std::vector<float>> storage, inStorage;
    float** buf = makeBuffer(storage, ch, n);
    float** inCopy = makeBuffer(inStorage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr);
    for (int s = 0; s < n; ++s) inCopy[0][s] = buf[0][s];

    SpectralFreezeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.5f, 0.0f, 0.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    // First 0.5s: deferred -> live copy of the input (old code: near-silent
    // frozen loop captured from the empty ring).
    float earlyDiff = 0.0f;
    for (int s = 0; s < sr / 2; ++s)
        earlyDiff += std::abs(buf[0][s] - inCopy[0][s]);
    REQUIRE(earlyDiff / static_cast<float>(sr / 2) < 1.0e-4f);
    // Last 0.5s: freeze engaged -> non-silent loop, distinct from the live copy.
    REQUIRE(peakAbs(buf, ch, n) > 0.1f);
    float lateDiff = 0.0f;
    for (int s = n - sr / 2; s < n; ++s)
        lateDiff += std::abs(buf[0][s] - inCopy[0][s]);
    REQUIRE(lateDiff / static_cast<float>(sr / 2) > 0.01f);
    delete[] buf; delete[] inCopy;
}

TEST_CASE("SpectralFreeze loop is continuous at the wrap", "[filter]")
{
    const int sr = 44100, ch = 1, n = 88200; // 2s past the 1s engage
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.8f);

    SpectralFreezeEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.5f, 0.0f, 0.0f, 0.0f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    // Frozen loop plays at pitch 1.0 -> 440Hz; slope <= 0.8*2*pi*440/44100
    // ~ 0.05. A hard splice at the wrap would produce a full-amplitude step.
    float maxDelta = 0.0f;
    for (int s = n - 44100 + 1; s < n; ++s)
        maxDelta = std::max(maxDelta, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(maxDelta < 0.30f);
    REQUIRE(peakAbs(buf, ch, n) > 0.1f);
    delete[] buf;
}

TEST_CASE("SpectralFreeze Offset picks the loop start position", "[filter]")
{
    // 1.5s ramp: at engage the 1s capture window holds the ramp from 0.5s to
    // 1.5s. Offset 0 starts at the window's beginning; Offset 0.5 starts in
    // the middle -> the frozen outputs differ by k * 0.5s at any fixed time.
    const int sr = 44100, ch = 1, n = 66150;
    const float k = 1.0e-4f;
    std::vector<std::vector<float>> storageA, storageB;
    float** bufA = makeBuffer(storageA, ch, n);
    float** bufB = makeBuffer(storageB, ch, n);
    for (int s = 0; s < n; ++s)
    {
        bufA[0][s] = k * static_cast<float>(s);
        bufB[0][s] = k * static_cast<float>(s);
    }

    SpectralFreezeEffect effA;
    effA.prepare(sr, ch);
    effA.reset();
    float paramsA[4] = { 0.5f, 0.0f, 0.0f, 0.0f };
    effA.processBlock(bufA, ch, n, paramsA);

    SpectralFreezeEffect effB;
    effB.prepare(sr, ch);
    effB.reset();
    float paramsB[4] = { 0.5f, 0.0f, 0.5f, 0.0f };
    effB.processBlock(bufB, ch, n, paramsB);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(allFinite(bufB, ch, n));
    // Sample at t = 1.3s (past the 1s engage + the 5ms entry fade; both read
    // heads still unwrapped).
    const int t = static_cast<int>(1.3 * sr);
    const float expected = k * static_cast<float>(sr) * 0.5f;
    REQUIRE(std::abs(bufB[0][t] - bufA[0][t] - expected) < k * 50.0f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("SpectralFreeze Offset repositions the loop live", "[filter]")
{
    // Changing the Offset while frozen must re-anchor the read head to the new
    // position (no freeze down/up needed).
    const int sr = 44100, ch = 1, n = 66150;
    const float k = 1.0e-4f;
    std::vector<std::vector<float>> storageA, storageB;
    float** bufA = makeBuffer(storageA, ch, n);
    float** bufB = makeBuffer(storageB, ch, n);
    for (int s = 0; s < n; ++s)
    {
        bufA[0][s] = k * static_cast<float>(s);
        bufB[0][s] = k * static_cast<float>(s);
    }

    SpectralFreezeEffect effA;
    effA.prepare(sr, ch);
    effA.reset();
    float paramsA[4] = { 0.5f, 0.0f, 0.5f, 0.0f };   // Offset 0.5 for the whole run
    effA.processBlock(bufA, ch, n, paramsA);

    SpectralFreezeEffect effB;
    effB.prepare(sr, ch);
    effB.reset();
    float paramsB1[4] = { 0.5f, 0.0f, 0.5f, 0.0f };
    const int split = static_cast<int>(1.2 * sr);
    effB.processBlock(bufB, ch, split, paramsB1);
    float paramsB2[4] = { 0.5f, 0.0f, 0.25f, 0.0f }; // Offset dragged to 0.25 at t=1.2s
    float* shiftedB[1] = { storageB[0].data() + split };
    effB.processBlock(shiftedB, ch, n - split, paramsB2);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(allFinite(bufB, ch, n));
    // At t = 1.4s: A's read = 0.5L + 0.4s; B's read = 0.25L + 0.2s (re-anchored
    // at 1.2s). The ramp difference = -k * (0.25L + 0.2s*sr) = -k * 19845.
    const int t = static_cast<int>(1.4 * sr);
    const float expected = k * (0.25f * static_cast<float>(sr) + 0.2f * static_cast<float>(sr));
    REQUIRE(std::abs(bufB[0][t] - bufA[0][t] + expected) < k * 50.0f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("Bitcrusher stays silent on silent input (dither gated)", "[bitcrush]")
{
    const int sr = 44100, ch = 1, n = 8820;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);

    BitcrusherEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 1.0f, 0.5f, 0.0f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(peakAbs(buf, ch, n) < 1e-4f);
    delete[] buf;
}

TEST_CASE("Bitcrusher produces non-silent bounded output on signal", "[bitcrush]")
{
    const int sr = 44100, ch = 1, n = 8820;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 440.0f, sr, 0.5f);

    BitcrusherEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 1.0f, 0.5f, 0.5f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    const float peak = peakAbs(buf, ch, n);
    REQUIRE(peak > 0.1f);
    REQUIRE(peak < 1.5f);
    delete[] buf;
}

TEST_CASE("ReverseBuffer stays click-free when the cycle exceeds the buffer", "[reverse]")
{
    // Density 0.5 gives a 2.6 s record+play cycle against a 1.5 s ring, so the
    // record head laps the slice mid-playback; exit transitions also must not
    // splice. Both must stay below the smooth-sine step bound.
    const int sr = 44100, ch = 1, n = sr * 12;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 220.0f, sr, 0.8f);

    ReverseEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.0f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    float maxStep = 0.0f;
    for (int s = 1; s < n; ++s)
        maxStep = std::max(maxStep, std::abs(buf[0][s] - buf[0][s - 1]));
    REQUIRE(maxStep < 0.15f);
    delete[] buf;
}

TEST_CASE("Reverse density polarity is natural (low = sparse, high = dense)", "[reverse]")
{
    const int sr = 44100, ch = 1, n = sr / 2; // 0.5s
    std::vector<std::vector<float>> storage, inStorage;
    float** buf = makeBuffer(storage, ch, n);
    float** inCopy = makeBuffer(inStorage, ch, n);
    fillSine(buf, ch, n, 220.0f, sr);
    for (int s = 0; s < n; ++s) inCopy[0][s] = buf[0][s];

    // Low density: 1.0s slice still recording -> live copy of the input.
    {
        ReverseEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        eff.processBlock(buf, ch, n, params);
        float diff = 0.0f;
        for (int s = 0; s < n; ++s)
            diff += std::abs(buf[0][s] - inCopy[0][s]);
        REQUIRE(allFinite(buf, ch, n));
        REQUIRE(diff / static_cast<float>(n) < 1.0e-4f);
    }

    // High density: 0.05s slice reversed and repeating within the first 0.1s.
    {
        fillSine(buf, ch, n, 220.0f, sr);
        ReverseEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        float params[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        eff.processBlock(buf, ch, n, params);
        float diff = 0.0f;
        for (int s = sr / 10; s < n; ++s)
            diff += std::abs(buf[0][s] - inCopy[0][s]);
        REQUIRE(allFinite(buf, ch, n));
        REQUIRE(diff / static_cast<float>(n - sr / 10) > 0.01f);
    }
    delete[] buf; delete[] inCopy;
}


TEST_CASE("VCA Compressor compresses loud input and stays bounded", "[dynamics]")
{
    const int sr = 44100, ch = 1, n = 88200;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 220.0f, sr, 0.8f);
    fillSine(bufB, ch, n, 220.0f, sr, 0.8f);

    float paramsComp[4] = { 0.0f, 0.0f, 0.75f, 0.0f };  // fast attack, thresh -15 dB, level 0.5 (neutral)
    float paramsOpen[4] = { 0.0f, 0.0f, 1.0f, 0.0f };   // thresh -5 dB -> env below threshold
    {
        VcaCompressorEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        eff.processBlock(bufA, ch, n, paramsComp);
    }
    {
        VcaCompressorEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        eff.processBlock(bufB, ch, n, paramsOpen);
    }

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(allFinite(bufB, ch, n));
    const float peakComp = peakAbs(bufA, ch, n);
    const float peakOpen = peakAbs(bufB, ch, n);
    REQUIRE(peakComp > 0.05f);
    REQUIRE(peakComp < 0.35f);       // ~6.75 dB of gain reduction below the 0.8 input
    REQUIRE(peakOpen > peakComp * 1.5f);
    REQUIRE(peakOpen < 0.6f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("Formant Shifter produces finite bounded output at maximum Q", "[filter]")
{
    const int sr = 44100, ch = 1, n = sr * 2;
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, 200.0f, sr, 0.8f);

    FormantShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.0f, 0.5f, 1.0f };  // formant mid, Q max
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    REQUIRE(peakAbs(buf, ch, n) > 1e-4f);
    REQUIRE(peakAbs(buf, ch, n) < 1.5f);
    delete[] buf;
}

TEST_CASE("Spectral Filter resonates at center and rejects off-center", "[filter]")
{
    const int sr = 44100, ch = 1, n = sr * 2;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 500.0f, sr, 0.5f);
    fillSine(bufB, ch, n, 500.0f, sr, 0.5f);

    float paramsCenter[4] = { 0.0f, 0.05f, 1.0f, 0.0f };  // center 500 Hz, Q max
    float paramsOff[4] = { 0.0f, 0.75f, 1.0f, 0.0f };     // center 6.1 kHz
    const auto lastPeak = [&](float** b) {
        float p = 0.0f;
        for (int s = n - sr; s < n; ++s)
            p = std::max(p, std::abs(b[0][s]));
        return p;
    };
    {
        SpectralFilterEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        eff.processBlock(bufA, ch, n, paramsCenter);
    }
    {
        SpectralFilterEffect eff;
        eff.prepare(sr, ch);
        eff.reset();
        eff.processBlock(bufB, ch, n, paramsOff);
    }

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(allFinite(bufB, ch, n));
    // Steady state only: the first block's center sweep fires a resonance chirp
    // as the narrow Q passes the input frequency (natural resonator behavior).
    REQUIRE(lastPeak(bufA) > 0.3f);
    REQUIRE(lastPeak(bufB) < 0.1f);
    delete[] bufA; delete[] bufB;
}

TEST_CASE("GrainScrubber produces non-silent output and position has an effect", "[granular]")
{
    const int sr = 44100, ch = 1, n = 30000;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    GrainScrubberEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float pLow[4]  = { 0.1f, 0.0f, 0.0f, 0.5f };
    float pHigh[4] = { 0.9f, 0.0f, 0.0f, 0.5f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(allFinite(bufB, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(peakAbs(bufB, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
}


TEST_CASE("ConvolutionReverb tail is filtered noise (not static white noise)", "[reverb]")
{
    const int sr = 44100, ch = 1, n = static_cast<int>(sr * 0.7f);
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    buf[0][0] = 1.0f;   // impulse: the output is the IR itself

    ConvolutionReverbEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { 0.0f, 0.5f, 0.5f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    REQUIRE(allFinite(buf, ch, n));
    float meanAbs = 0.0f, meanDelta = 0.0f;
    int count = 0;
    for (int s = static_cast<int>(sr * 0.03f); s < n - 1; ++s)
    {
        meanAbs += std::abs(buf[0][s]);
        meanDelta += std::abs(buf[0][s + 1] - buf[0][s]);
        ++count;
    }
    meanAbs /= count;
    meanDelta /= count;
    const float ratio = (meanAbs > 1.0e-6f) ? meanDelta / meanAbs : 0.0f;
    REQUIRE(peakAbs(buf, ch, n) > 0.1f);
    // The one-pole filtered tail measures ~1.1 (adjacent-sample correlation);
    // the old white-noise IR tail measured ~1.4 (i.i.d. samples -> static-ish).
    REQUIRE(ratio < 1.25f);
    delete[] buf;
}

TEST_CASE("FrequencyShifter suppresses the image sideband", "[pitch]")
{
    const int sr = 44100, ch = 1, n = sr;
    const float fIn = 1000.0f, shiftHz = 200.0f;
    const float shiftParam = shiftHz / 2000.0f;              // shiftHz = p * 2000 (linear)
    std::vector<std::vector<float>> storage;
    float** buf = makeBuffer(storage, ch, n);
    fillSine(buf, ch, n, fIn, sr);

    FrequencyShifterEffect eff;
    eff.prepare(sr, ch);
    eff.reset();
    float params[4] = { shiftParam, 0.0f, 0.5f, 0.5f };
    eff.processBlock(buf, ch, n, params);

    const auto magAt = [&](float f) {
        float re = 0.0f, im = 0.0f;
        for (int s = n / 2; s < n; ++s)
        {
            const float ph = 6.2831853f * f * static_cast<float>(s) / sr;
            re += buf[0][s] * std::cos(ph);
            im += buf[0][s] * std::sin(ph);
        }
        return std::sqrt(re * re + im * im);
    };
    const float mWanted = magAt(fIn + shiftHz);
    const float mImage = magAt(fIn - shiftHz);
    REQUIRE(allFinite(buf, ch, n));
    // 8-section phase-difference network: image suppression across
    // the band (the broken state gave an equal-strength double sideband).
    REQUIRE(mWanted > 0.1f);
    REQUIRE(mImage / mWanted < 0.1f);
    delete[] buf;
}
