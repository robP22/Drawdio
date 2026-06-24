#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>
#include <array>

constexpr int GridSize = 256;
constexpr int TotalCells = GridSize * GridSize;
constexpr int PedalSlotCount = 6;

// Memory safety limits for DSP delay buffers
constexpr size_t MaxReverbDelayMs = 5000;   // 5 second max reverb delay
constexpr double MaxGranularDurationSec = 10.0;  // 10 second max granular buffer
constexpr size_t MaxSimpleDelaySec = 2;     // 2 second max simple delay

inline float colorWeight(uint8_t pixelVal)
{
    switch (pixelVal)
    {
        case 5:  return -1.0f;   // Black   (paired with Brown)
        case 7:  return  0.9f;   // Brown   (paired with Black)
        case 8:  return -0.55f;  // Purple  (paired with Violet)
        case 12: return  0.6f;   // Violet  (paired with Purple)
        case 1:  return -0.8f;   // Blue    (paired with Green)
        case 2:  return  0.55f;  // Green   (paired with Blue)
        case 9:  return  0.0f;   // Grey    (neutral)
        case 10: return -0.6f;   // Pink    (paired with Red)
        case 6:  return  0.7f;   // Yellow  (paired with Orange)
        case 11: return -0.9f;   // Orange  (paired with Yellow)
        case 3:  return  0.8f;   // Red     (paired with Pink)
        case 4:  return -0.7f;   // White   (bright)
        default: return  0.0f;
    }
}

inline float interpolateDelayRead(const std::vector<float>& buf, float pos) 
{
    size_t idx = static_cast<size_t>(pos) % buf.size();
    float frac = pos - std::floor(pos);
    size_t next = (idx + 1) % buf.size();
    return buf[idx] * (1.0f - frac) + buf[next] * frac;
}

enum class DspModuleType : uint8_t
{
    BYPASS = 0,
    WAVESHAPER_DISTORTION,
    MICROPITCH_CHORUS,
    MULTI_MODE_FILTER,
    PITCH_SHIFTER_GRANULAR,
    ENVELOPE_VCA_COMPRESSOR,
    GLITCH_STUTTER,
    DIFFUSED_DELAY_NETWORK,
    MATHEMATICAL_WAVEFOLDER,
    FORMANT_VOCAL_SHIFTER,
    TAPE_STOP_REVERSE_ECHO,
    SIMPLE_DELAY,
    PLATE_REVERB,
    SIDECHAIN_DUCKER,
    GRANULAR_DELAY,
    COMB_RESONATOR,
    SPECTRAL_FREEZE,
    FREQ_SHIFTER,
    REVERSE_BUFFER,
    GRAIN_SCRUBBER,
    SPECTRAL_FILTER,
    CONVOLUTION_SPACE,
    RANDOM_MODULATOR
};

// Parameter token identifiers used across the system.
namespace ParamToken {
    constexpr uint16_t Knob0 = 0;
    constexpr uint16_t Knob1 = 1;
    constexpr uint16_t Knob2 = 2;
    constexpr uint16_t Knob3 = 3;
}

struct ParameterDescriptor
{
    uint16_t parameterToken;
    float minValue;
    float maxValue;
    float defaultValue;
    float currentValue;
    bool isManualOverride = false;
    uint8_t targetDspNodeRegister;
};

struct PedalAssetPayload
{
    std::vector<DspModuleType> activeRoutingChain;
    std::vector<uint8_t> routingSlotOrder; // maps sorted-chain position -> original slot index (0-5)
    std::vector<ParameterDescriptor> parameters;
};

struct PedalRowRange
{
    int startRow;
    int numRows;
};

// -------------------------------------------------------------------
// Compute evenly-spaced row divisions for N active pedal slots.
inline std::vector<PedalRowRange> calculateRowRanges(int activeCount)
{
    if (activeCount <= 0) return {};

    std::vector<PedalRowRange> ranges(static_cast<size_t>(activeCount));
    int baseSlice = GridSize / activeCount;
    int remainder = GridSize % activeCount;
    int currentRow = 0;
    for (int i = 0; i < activeCount; ++i)
    {
        ranges[static_cast<size_t>(i)].startRow = currentRow;
        ranges[static_cast<size_t>(i)].numRows = baseSlice + (i < remainder ? 1 : 0);
        currentRow += ranges[static_cast<size_t>(i)].numRows;
    }
    return ranges;
}

// -------------------------------------------------------------------
// Accumulate weighted pixel contributions from a row-range sub-region
// and return a normalised value in [0, 1].
inline float calculatePixelAccumulation(const std::array<uint8_t, TotalCells>& gridData,
                                         const PedalRowRange& range,
                                         int parameterIndex,
                                         int totalParameters)
{
    if (range.numRows == 0 || totalParameters == 0)
        return 0.5f;

    int cellsPerParam = (GridSize * range.numRows) / totalParameters;
    int startCell = parameterIndex * cellsPerParam;
    int endCell = (parameterIndex == totalParameters - 1)
                      ? (GridSize * range.numRows)
                      : (startCell + cellsPerParam);

    float accumulator = 0.0f;
    float sumAbs = 0.0f;
    int paintedCount = 0;
    int totalCells = 0;
    bool colorPresent[13] = {false};

    for (int localCell = startCell; localCell < endCell; ++localCell)
    {
        int gridX = localCell % GridSize;
        int gridY = range.startRow + (localCell / GridSize);
        if (gridY >= GridSize) break;

        uint8_t val = gridData[gridY * GridSize + gridX];
        ++totalCells;
        if (val != 0)
        {
            ++paintedCount;
            float w = colorWeight(val);
            accumulator += w;
            sumAbs += std::abs(w);
            colorPresent[val] = true;
        }
    }

    if (totalCells == 0 || paintedCount == 0)
        return 0.5f;

    int uniqueCount = 0;
    for (int i = 1; i <= 12; ++i)
        if (colorPresent[i]) ++uniqueCount;

    float coverage = static_cast<float>(paintedCount) / static_cast<float>(totalCells);
    float avgWeight = accumulator / static_cast<float>(paintedCount);
    float avgAbs = sumAbs / static_cast<float>(paintedCount);
    float diversity = static_cast<float>(uniqueCount) / 12.0f;
    float bias = 0.5f + avgWeight * (0.25f + diversity * 0.5f + avgAbs * 0.25f);
    float result = bias * coverage + 0.5f * (1.0f - coverage);

    return (result < 0.0f) ? 0.0f : (result > 1.0f ? 1.0f : result);
}

// -------------------------------------------------------------------
// Configuration for a comb + allpass reverb network.
struct ReverbNetworkConfig
{
    double feedbackBase;
    double feedbackRange;
    std::array<float, 4> combGains;
    float apCoeff;
    std::array<int, 4> combTimesMs;
    std::array<int, 2> apTimesMs;
};

// Reverb state (4 comb filters + 2 allpass filters + stereo decorrelation).
struct ReverbNetworkState
{
    std::vector<float> combBuf[4];
    size_t combPtr[4];
    float combDampState[4];
    std::vector<float> apBuf[2];
    size_t apPtr[2];
    float decorrL;
    float decorrR;
    float combLfoPhase[4];
    std::vector<float> reflectBuf[2];
    size_t reflectPtr;
};

// Allocate reverb delay lines from sample rate.
inline void prepareReverbNetwork(ReverbNetworkState& state, double sampleRate,
                                  const ReverbNetworkConfig& config)
{
    for (int i = 0; i < 4; ++i)
    {
        size_t clampedTimeMs = std::min(static_cast<size_t>(config.combTimesMs[i]), MaxReverbDelayMs);
        size_t len = static_cast<size_t>(sampleRate * clampedTimeMs / 1000.0);
        state.combBuf[i].assign(len, 0.0f);
        state.combPtr[i] = 0;
    }
    for (int i = 0; i < 2; ++i)
    {
        size_t clampedTimeMs = std::min(static_cast<size_t>(config.apTimesMs[i]), MaxReverbDelayMs);
        size_t len = static_cast<size_t>(sampleRate * clampedTimeMs / 1000.0);
        state.apBuf[i].assign(len, 0.0f);
        state.apPtr[i] = 0;
    }
    for (int i = 0; i < 4; ++i)
        state.combDampState[i] = 0.0f;
    for (int i = 0; i < 4; ++i)
        state.combLfoPhase[i] = static_cast<float>(i) * 1.5707963f;
    for (int i = 0; i < 2; ++i)
        state.reflectBuf[i].assign(3072, 0.0f);
    state.reflectPtr = 0;
    state.decorrL = 0.0f;
    state.decorrR = 0.0f;
}

// Reset reverb delay lines to zero.
inline void resetReverbNetwork(ReverbNetworkState& state)
{
    for (int i = 0; i < 4; ++i)
    {
        std::fill(state.combBuf[i].begin(), state.combBuf[i].end(), 0.0f);
        state.combPtr[i] = 0;
    }
    for (int i = 0; i < 2; ++i)
    {
        std::fill(state.apBuf[i].begin(), state.apBuf[i].end(), 0.0f);
        state.apPtr[i] = 0;
    }
    for (int i = 0; i < 4; ++i)
        state.combDampState[i] = 0.0f;
    for (int i = 0; i < 2; ++i)
        std::fill(state.reflectBuf[i].begin(), state.reflectBuf[i].end(), 0.0f);
    state.reflectPtr = 0;
    state.decorrL = 0.0f;
    state.decorrR = 0.0f;
}

// Process one sample through a comb + allpass reverb network.
// dryL / dryR should be the sample value before the reverberant mix.
// outL / outR receive the wet signal with stereo decorrelation.
inline void processReverbNetworkSample(float dryL, float dryR,
                                        const ReverbNetworkConfig& config,
                                        ReverbNetworkState& state,
                                        float decayNormalised,
                                        float& outL, float& outR)
{
    juce::ScopedNoDenormals noDenorm;
    float feedback = static_cast<float>(config.feedbackBase + decayNormalised * config.feedbackRange);
    float hfDamp = decayNormalised * decayNormalised * 0.5f;

    static constexpr size_t kTapOffsets[5] = { 882, 1102, 1411, 1852, 2426 };
    static constexpr float kTapGains[5] = { 0.32f, 0.20f, 0.13f, 0.07f, 0.03f };

    size_t rLen = state.reflectBuf[0].size();
    state.reflectBuf[0][state.reflectPtr] = dryL;
    state.reflectBuf[1][state.reflectPtr] = dryR;

    float erL = 0.0f, erR = 0.0f;
    for (int t = 0; t < 5; ++t)
    {
        size_t idx = (state.reflectPtr + rLen - kTapOffsets[t]) % rLen;
        erL += state.reflectBuf[0][idx] * kTapGains[t];
        erR += state.reflectBuf[1][idx] * kTapGains[t];
    }
    state.reflectPtr = (state.reflectPtr + 1) % rLen;

    float monoIn = (erL + erR) * 0.5f;

    float combOut = 0.0f;
    static constexpr float kModInc[4] = { 0.0000869f, 0.000124f, 0.000161f, 0.000209f };
    for (int i = 0; i < 4; ++i)
    {
        size_t bufLen = state.combBuf[i].size();
        if (bufLen == 0) continue;

        state.combLfoPhase[i] += kModInc[i];
        if (state.combLfoPhase[i] > 2.0f * 3.14159265f)
            state.combLfoPhase[i] -= 2.0f * 3.14159265f;

        float modPos = static_cast<float>(state.combPtr[i]) + std::sin(state.combLfoPhase[i]) * 1.2f;
        float tap = interpolateDelayRead(state.combBuf[i], modPos);
        tap = state.combDampState[i] = state.combDampState[i] + hfDamp * (tap - state.combDampState[i]);
        state.combBuf[i][state.combPtr[i]] = monoIn + std::tanh(tap * feedback * config.combGains[i]) * 0.7f;
        combOut += tap;
        state.combPtr[i] = (state.combPtr[i] + 1) % bufLen;
    }
    combOut *= 0.25f;

    for (int i = 0; i < 2; ++i)
    {
        size_t bufLen = state.apBuf[i].size();
        if (bufLen == 0) continue;

        float& bufVal = state.apBuf[i][state.apPtr[i]];
        float tap = bufVal;
        bufVal = combOut + config.apCoeff * tap;
        combOut = -config.apCoeff * combOut + tap;
        state.apPtr[i] = (state.apPtr[i] + 1) % bufLen;
    }

    // Allpass decorrelation for stereo spread.
    float k = 0.25f;
    outL = -k * combOut + state.decorrL;
    state.decorrL = combOut + k * outL;

    outR = -k * combOut + state.decorrR;
    state.decorrR = combOut + k * outR;
}

// -------------------------------------------------------------------
// State for a granular-style delay processor.
struct GranularProcessorState
{
    std::vector<float> delayBuf;
    size_t writePtr;
    float readPtr;
    float grainPhase2;
    float grain2Pos;
    int grainLen;
    size_t grainBase;
    std::vector<float> window;
    uint32_t rngState = 12345;
};

// Prepare a granular delay line.
inline void prepareGranularProcessor(GranularProcessorState& state,
                                      double sampleRate, double durationSec)
{
    double safeDuration = std::min(durationSec, MaxGranularDurationSec);
    size_t size = static_cast<size_t>(sampleRate * safeDuration);
    state.delayBuf.assign(size, 0.0f);
    state.window.assign(size, 0.0f);
    state.writePtr = 0;
    state.readPtr = 0.0f;
    state.grainPhase2 = 0.0f;
    state.grain2Pos = 0.0f;
    state.grainLen = 0;
    state.grainBase = 0;
}

// Reset a granular delay line to silence.
inline void resetGranularProcessor(GranularProcessorState& state)
{
    std::fill(state.delayBuf.begin(), state.delayBuf.end(), 0.0f);
    state.writePtr = 0;
    state.readPtr = 0.0f;
    state.grainPhase2 = 0.0f;
    state.grain2Pos = 0.0f;
    state.grainLen = 0;
    state.grainBase = 0;
}

// Process one sample through granular pitch shifting / delay.
// sampleRate used for grain-length calculation when state.grainLen == 0.
inline float processGranularSample(float input, GranularProcessorState& state,
                                    float playbackSpeed, double sampleRate,
                                    float grainDurationSec, float grainPosition = 0.0f)
{
    size_t bufSize = state.delayBuf.size();
    if (bufSize == 0) return 0.0f;

    if (state.grainLen == 0 || state.readPtr >= static_cast<float>(state.grainLen))
    {
        state.grainLen = std::max(1, static_cast<int>(sampleRate * grainDurationSec));
        state.readPtr = 0.0f;
        state.grainPhase2 = static_cast<float>(state.grainLen) * 0.5f;

        state.rngState ^= state.rngState << 13;
        state.rngState ^= state.rngState >> 17;
        state.rngState ^= state.rngState << 5;
        float spray = (static_cast<float>(state.rngState & 0x3FFF) / 16383.0f - 1.0f) * 0.10f;

        state.grainBase = (state.writePtr + bufSize - static_cast<size_t>(state.grainLen)
                           - static_cast<size_t>((grainPosition + spray) * static_cast<float>(bufSize))) % bufSize;
        size_t wLen = std::min(static_cast<size_t>(state.grainLen), state.window.size());
        for (size_t wi = 0; wi < wLen; ++wi)
        {
            float phase = static_cast<float>(wi) / static_cast<float>(state.grainLen);
            state.window[wi] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * phase));
        }
    }

    if (!std::isfinite(input)) input = 0.0f;
    state.delayBuf[state.writePtr] = input;

    float grainLenF = static_cast<float>(state.grainLen);
    float bufSizeF = static_cast<float>(bufSize);

    float pos1 = static_cast<float>(state.grainBase) + state.readPtr;
    if (pos1 >= bufSizeF) pos1 -= bufSizeF;

    float pos2 = state.grain2Pos;
    if (pos2 >= bufSizeF) pos2 -= bufSizeF;

    float s1 = interpolateDelayRead(state.delayBuf, pos1);

    float s2 = interpolateDelayRead(state.delayBuf, pos2);

    size_t wi1 = std::min(static_cast<size_t>(state.readPtr),
                          static_cast<size_t>(state.grainLen) - 1);
    size_t wi2 = std::min(static_cast<size_t>(state.grainPhase2),
                          static_cast<size_t>(state.grainLen) - 1);
    float w1 = state.window[wi1];
    float w2 = state.window[wi2];

    float out = s1 * w1 + s2 * w2;

    state.readPtr += playbackSpeed;

    state.grainPhase2 += playbackSpeed;
    if (state.grainPhase2 >= grainLenF)
        state.grainPhase2 -= grainLenF;

    state.grain2Pos += playbackSpeed;
    if (state.grain2Pos >= bufSizeF)
        state.grain2Pos -= bufSizeF;

    state.writePtr = (state.writePtr + 1) % bufSize;

    return out;
}

// -------------------------------------------------------------------
// Simple circular delay buffer state.
struct SimpleDelayState
{
    std::vector<float> buf;
    size_t writePtr;
};

inline void prepareSimpleDelay(SimpleDelayState& state, double sampleRate, double durationSec)
{
    double safeDuration = std::min(durationSec, static_cast<double>(MaxSimpleDelaySec));
    size_t size = static_cast<size_t>(sampleRate * safeDuration);
    state.buf.assign(size, 0.0f);
    state.writePtr = 0;
}

inline void resetSimpleDelay(SimpleDelayState& state)
{
    std::fill(state.buf.begin(), state.buf.end(), 0.0f);
    state.writePtr = 0;
}
