#include <catch2/catch_test_macros.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "State/ConfigManager.h"
#include "UnifiedPedalProcessor.h"

namespace
{

// processAudioBlock clamps to maxSamplesPerBlock (512) and writes from sample 0
// of the passed buffer, so process the full duration in shifted 512-sample
// blocks. Keep the total a multiple of 512 so the last block stays in bounds.
constexpr int kTotalSamples = 512 * 173; // 88576 (~2.01 s)
constexpr int kBlockSamples = 512;
constexpr int kMeasureWindow = 44100;

bool waitForCompiledResult(ConfigManager& cm, int timeoutMs = 8000)
{
    const auto start = std::chrono::steady_clock::now();
    while (!cm.consumeCompiledResultIfAvailable())
    {
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeoutMs))
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

struct TestBuffers
{
    std::vector<std::vector<float>> storage;
    std::vector<float*> ptrs;
    float** buf = nullptr;

    explicit TestBuffers(int ch)
    {
        storage.resize(static_cast<size_t>(ch));
        for (auto& c : storage) c.assign(static_cast<size_t>(kTotalSamples), 0.0f);
        ptrs.resize(static_cast<size_t>(ch));
        for (int i = 0; i < ch; ++i) ptrs[static_cast<size_t>(i)] = storage[static_cast<size_t>(i)].data();
        buf = ptrs.data();
    }
};

void fillSine(float** b, int ch, int n, float freq, int sr, float amp = 0.8f)
{
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
            b[c][s] = amp * std::sin(6.283185307179586f * freq * static_cast<float>(s) / static_cast<float>(sr));
}

bool allFinite(float** b, int ch, int n)
{
    for (int c = 0; c < ch; ++c)
        for (int s = 0; s < n; ++s)
            if (!std::isfinite(b[c][s])) return false;
    return true;
}

// Ratio of the quietest to the loudest 1000-sample segment peak over the last
// second; ~1.0 for unmodulated audio, ~(1 - depth) for tremolo at depth d.
float envelopeRatio(float** b, int ch, int n, int window, int segSamples)
{
    float maxSeg = 0.0f;
    float minSeg = 1e9f;
    const int startIdx = n - window;
    for (int start = startIdx; start + segSamples <= n; start += segSamples)
    {
        float segPeak = 0.0f;
        for (int s = start; s < start + segSamples; ++s)
            for (int c = 0; c < ch; ++c)
                segPeak = std::max(segPeak, std::abs(b[c][s]));
        maxSeg = std::max(maxSeg, segPeak);
        minSeg = std::min(minSeg, segPeak);
    }
    return maxSeg > 1e-9f ? minSeg / maxSeg : 1.0f;
}

float compiledParam(const PedalAssetPayload* cfg, uint8_t chainPos, uint16_t token)
{
    for (const auto& p : cfg->parameters)
        if (p.targetDspNodeRegister == chainPos && p.parameterToken == token)
            return p.currentValue;
    return -1.0f;
}

void processTwoSeconds(UnifiedPedalProcessor& dsp, ConfigManager& cm, TestBuffers& tb, int ch)
{
    auto view = cm.getAudioView();
    std::vector<float*> shifted(static_cast<size_t>(ch));
    for (int off = 0; off < kTotalSamples; off += kBlockSamples)
    {
        for (int c = 0; c < ch; ++c)
            shifted[static_cast<size_t>(c)] = tb.storage[static_cast<size_t>(c)].data() + off;
        dsp.processAudioBlock(shifted.data(), ch, kBlockSamples, view);
    }
}

float lastSecondEnvelopeRatio(TestBuffers& tb, int ch)
{
    return envelopeRatio(tb.buf, ch, kTotalSamples, kMeasureWindow, 100);
}

float lastSecondPeak(TestBuffers& tb, int ch)
{
    float p = 0.0f;
    for (int c = 0; c < ch; ++c)
        for (int s = kTotalSamples - kMeasureWindow; s < kTotalSamples; ++s)
            p = std::max(p, std::abs(tb.buf[c][s]));
    return p;
}

} // namespace

TEST_CASE("Fresh Tremolo in manual mode is audible at default depth", "[manualmode]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager cm(dsp);
    cm.prepare(44100.0, 512);

    cm.setManualMode(true);
    cm.setPedalSlot(0, DspModuleType::TREMOLO);
    REQUIRE(waitForCompiledResult(cm));

    auto knobVals = cm.getKnobValues();
    REQUIRE(std::abs(knobVals[0 * KnobsPerPedal + 2] - 0.5f) < 1e-4f);

    TestBuffers tb(1);
    fillSine(tb.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tb, 1);

    REQUIRE(allFinite(tb.buf, 1, kTotalSamples));
    REQUIRE(lastSecondEnvelopeRatio(tb, 1) < 0.85f);
    REQUIRE(lastSecondEnvelopeRatio(tb, 1) > 0.3f);
}

TEST_CASE("Manual mode ignores canvas pixels for params", "[manualmode]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager cm(dsp);
    cm.prepare(44100.0, 512);

    std::array<uint8_t, TotalCells> brownGrid{};
    brownGrid.fill(7);
    cm.setGridData(brownGrid);
    cm.setManualMode(true);
    cm.setPedalSlot(0, DspModuleType::TREMOLO);
    REQUIRE(waitForCompiledResult(cm));

    auto knobVals = cm.getKnobValues();
    REQUIRE(std::abs(knobVals[0 * KnobsPerPedal + 2] - 0.5f) < 1e-4f);

    TestBuffers tb(1);
    fillSine(tb.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tb, 1);

    REQUIRE(allFinite(tb.buf, 1, kTotalSamples));
    REQUIRE(lastSecondEnvelopeRatio(tb, 1) > 0.3f);
    REQUIRE(lastSecondEnvelopeRatio(tb, 1) < 0.85f);
}

TEST_CASE("Canvas-to-manual switch retains compiled params", "[manualmode]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager cm(dsp);
    cm.prepare(44100.0, 512);

    std::array<uint8_t, TotalCells> brownGrid{};
    brownGrid.fill(7);
    cm.setGridData(brownGrid);
    cm.setPedalSlot(0, DspModuleType::TREMOLO);
    REQUIRE(waitForCompiledResult(cm));

    const float compiledDepth = compiledParam(cm.getCurrentConfig(), 0, ParamToken::Knob2);
    REQUIRE(compiledDepth > 0.6f);
    REQUIRE(compiledDepth < 0.8f);

    TestBuffers tbA(1);
    fillSine(tbA.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tbA, 1);
    const float canvasRatio = lastSecondEnvelopeRatio(tbA, 1);
    REQUIRE(canvasRatio < 0.85f);

    cm.setManualMode(true);
    auto knobVals = cm.getKnobValues();
    REQUIRE(std::abs(knobVals[0 * KnobsPerPedal + 2] - compiledDepth) < 1e-4f);

    cm.setManualRouting({0});
    REQUIRE(waitForCompiledResult(cm));

    TestBuffers tbB(1);
    fillSine(tbB.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tbB, 1);
    REQUIRE(cm.getCurrentConfig()->manualParams);
    const float manualRatio = lastSecondEnvelopeRatio(tbB, 1);
    REQUIRE(manualRatio < 0.85f);
    REQUIRE(std::abs(manualRatio - canvasRatio) < 0.1f);
}

TEST_CASE("Dragged knob override survives canvas-to-manual switch", "[manualmode]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager cm(dsp);
    cm.prepare(44100.0, 512);

    std::array<uint8_t, TotalCells> brownGrid{};
    brownGrid.fill(7);
    cm.setGridData(brownGrid);
    cm.setPedalSlot(0, DspModuleType::TREMOLO);
    REQUIRE(waitForCompiledResult(cm));

    cm.setKnobParameter(0, 2, 0.5f, 0.9f);
    REQUIRE(cm.isParamOverridden(0, 2));

    cm.setManualMode(true);
    REQUIRE(cm.isParamOverridden(0, 2));
    auto knobVals = cm.getKnobValues();
    REQUIRE(std::abs(knobVals[0 * KnobsPerPedal + 2] - 0.9f) < 1e-4f);

    cm.setManualRouting({0});
    REQUIRE(waitForCompiledResult(cm));

    TestBuffers tb(1);
    fillSine(tb.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tb, 1);

    REQUIRE(allFinite(tb.buf, 1, kTotalSamples));
    REQUIRE(lastSecondEnvelopeRatio(tb, 1) < 0.3f);
}

TEST_CASE("Canvas mode uses compiled params and mask overrides", "[manualmode]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager cm(dsp);
    cm.prepare(44100.0, 512);

    cm.setPedalSlot(0, DspModuleType::TREMOLO);
    REQUIRE(waitForCompiledResult(cm));
    REQUIRE_FALSE(cm.getCurrentConfig()->manualParams);

    TestBuffers tbA(1);
    fillSine(tbA.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tbA, 1);
    const float baseRatio = lastSecondEnvelopeRatio(tbA, 1);
    REQUIRE(baseRatio > 0.3f);
    REQUIRE(baseRatio < 0.85f);

    cm.setKnobParameter(0, 2, 0.5f, 0.9f);
    TestBuffers tbB(1);
    fillSine(tbB.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tbB, 1);
    REQUIRE(lastSecondEnvelopeRatio(tbB, 1) < 0.3f);
}

TEST_CASE("Bypassing a mid-chain effect stops its processing", "[manualmode]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager cm(dsp);
    cm.prepare(44100.0, 512);

    cm.setPedalSlot(0, DspModuleType::TREMOLO);
    REQUIRE(waitForCompiledResult(cm));

    TestBuffers tbA(1);
    fillSine(tbA.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tbA, 1);
    const float basePeak = lastSecondPeak(tbA, 1);
    const float baseRatio = lastSecondEnvelopeRatio(tbA, 1);
    REQUIRE(basePeak > 0.6f);
    REQUIRE(baseRatio > 0.3f);
    REQUIRE(baseRatio < 0.85f);

    cm.setPedalSlot(1, DspModuleType::WAVESHAPER);
    REQUIRE(waitForCompiledResult(cm));

    // Bypass before the Waveshaper config finishes its crossfade: the new
    // config is deferred until the audio thread clears m_nextConfig.
    cm.setPedalSlot(1, DspModuleType::BYPASS);
    REQUIRE(waitForCompiledResult(cm));

    TestBuffers tbB(1);
    fillSine(tbB.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tbB, 1);
    const float midPeak = lastSecondPeak(tbB, 1);
    // The Waveshaper config became current after its crossfade, so the mid run
    // legitimately processes it; keep a wide margin (peak compression is small).
    REQUIRE(midPeak < basePeak * 0.90f);

    cm.tryApplyDeferredConfig();
    TestBuffers tbC(1);
    fillSine(tbC.buf, 1, kTotalSamples, 440.0f, 44100);
    processTwoSeconds(dsp, cm, tbC, 1);
    const float finalPeak = lastSecondPeak(tbC, 1);
    const float finalRatio = lastSecondEnvelopeRatio(tbC, 1);
    REQUIRE(allFinite(tbC.buf, 1, kTotalSamples));
    REQUIRE(finalPeak > basePeak * 0.85f);
    REQUIRE(std::abs(finalRatio - baseRatio) < 0.12f);
}

TEST_CASE("Importing a new image recompiles the routing", "[manualmode]")
{
    UnifiedPedalProcessor dsp;
    dsp.prepareToPlay(44100.0, 512, 1);
    ConfigManager cm(dsp);
    cm.prepare(44100.0, 512);

    cm.setPedalSlot(0, DspModuleType::TREMOLO);
    cm.setPedalSlot(2, DspModuleType::WAVESHAPER);
    cm.setPedalSlot(4, DspModuleType::GLITCH_STUTTER);
    REQUIRE(waitForCompiledResult(cm));

    std::array<uint8_t, TotalCells> imageA{};
    std::array<uint8_t, TotalCells> imageB{};
    // A: slot 0's band painted far left (score ~0), slot 2's band far right
    // (high x-mean score) -> the right-biased band sorts last: {0, 4, 2}.
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 10; ++x)
            imageA[static_cast<size_t>(y) * GridSize + x] = 3;
    for (int y = 86; y < 126; ++y)
        for (int x = 206; x < GridSize; ++x)
            imageA[static_cast<size_t>(y) * GridSize + x] = 3;
    // B: swapped - slot 2's band far left, slot 0's band far right.
    for (int y = 86; y < 126; ++y)
        for (int x = 0; x < 10; ++x)
            imageB[static_cast<size_t>(y) * GridSize + x] = 3;
    for (int y = 0; y < 40; ++y)
        for (int x = 206; x < GridSize; ++x)
            imageB[static_cast<size_t>(y) * GridSize + x] = 3;

    // Complete the A config's crossfade so its nextConfig slot frees up; the
    // plugin does this via the editor tick + running audio (deferred configs
    // apply through tryApplyDeferredConfig once the audio thread clears it).
    auto settleAudio = [&](ConfigManager& cfg, UnifiedPedalProcessor& dspProc) {
        auto view = cfg.getAudioView();
        TestBuffers tb(1);
        for (int i = 0; i < 4; ++i)
            dspProc.processAudioBlock(tb.buf, 1, kBlockSamples, view);
        cfg.tryApplyDeferredConfig();
    };

    cm.submitCanvasSnapshot(imageA);
    REQUIRE(waitForCompiledResult(cm));
    settleAudio(cm, dsp);
    const auto orderA = cm.getLastConfigSync().routingSlotOrder;

    cm.submitCanvasSnapshot(imageB);
    REQUIRE(waitForCompiledResult(cm));
    settleAudio(cm, dsp);
    const auto orderB = cm.getLastConfigSync().routingSlotOrder;

    REQUIRE(orderA.size() == 3);
    REQUIRE(orderB.size() == 3);
    auto sortedB = orderB;
    std::sort(sortedB.begin(), sortedB.end());
    REQUIRE(sortedB == std::vector<uint8_t>({ 0, 2, 4 }));
    REQUIRE(orderA.size() == 3);
    REQUIRE(orderB.size() == 3);
    REQUIRE(orderA != orderB);
    // Empty band scores 0 and sorts first; right-biased (high x-mean) sorts last.
    REQUIRE(orderA == std::vector<uint8_t>({ 4, 0, 2 }));
    REQUIRE(orderB == std::vector<uint8_t>({ 4, 2, 0 }));
}
