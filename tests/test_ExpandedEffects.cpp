#include <catch2/catch_test_macros.hpp>
#include "Effects/DistortionEffects.h"
#include "Effects/ReverbEffects.h"
#include "Effects/ModulationEffects.h"
#include "Effects/PitchEffects.h"
#include "Effects/ReTimeEffect.h"
#include "Effects/ReverseEffect.h"
#include "Effects/HpLpFilterEffect.h"
#include "Effects/BitcrusherEffect.h"
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
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pHigh[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
    eff.processBlock(bufA, ch, n, pLow);
    eff.processBlock(bufB, ch, n, pHigh);

    REQUIRE(allFinite(bufA, ch, n));
    REQUIRE(peakAbs(bufA, ch, n) > 1e-4f);
    REQUIRE(sumAbsDiff(bufA, bufB, ch, n) > 1e-3f);
    delete[] bufA; delete[] bufB;
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
    float pLow[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };
    float pHigh[4] = { 0.0f, 0.0f, 0.0f, 0.95f };
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

TEST_CASE("FrequencyShifter produces non-silent output and shift has an effect", "[pitch]")
{
    const int sr = 44100, ch = 1, n = 4096;
    std::vector<std::vector<float>> sA, sB;
    float** bufA = makeBuffer(sA, ch, n);
    float** bufB = makeBuffer(sB, ch, n);
    fillSine(bufA, ch, n, 440.0f, sr);
    fillSine(bufB, ch, n, 440.0f, sr);

    FrequencyShifterEffect eff;
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

// ---------------------------------------------------------------------------
TEST_CASE("ReTime produces non-silent finite output and speed has an effect", "[retime]")
{
    // ReTime is a looper: it replays audio recorded roughly one loop length
    // behind the write head, and the read window is anchored in a fixed region
    // of an 8-second buffer. So warm up past ~352800 samples before checking.
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

    // Inspect only the portion well after the loop window has filled.
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
