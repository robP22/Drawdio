#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>
#include <array>

constexpr int GridSize = 128;
constexpr int TotalCells = GridSize * GridSize;
constexpr int PedalSlotCount = 6;

// Memory safety limits for DSP delay buffers
constexpr size_t MaxReverbDelayMs = 5000;   // 5 second max reverb delay
constexpr double MaxGranularDurationSec = 10.0;  // 10 second max granular buffer
constexpr size_t MaxSimpleDelaySec = 10;    // 10 second max simple delay
constexpr size_t MaxRingBufferSec = 10;      // 10 second max ring buffer

inline float colorWeight(uint8_t pixelVal)
{
    switch (pixelVal)
    {
        case 0: return -2.0f;
        case 1: return -1.0f;
        case 2: return  0.0f;
        case 3: return  1.0f;
        case 4: return  2.0f;
        default: return -2.0f;
    }
}

inline float signedPixelContribution(uint8_t pixelVal, int cellIndex)
{
    float w = colorWeight(pixelVal);
    return (cellIndex % 2 == 0) ? w : -w;
}

enum class DspModuleType : uint8_t
{
    BYPASS = 0,
    WAVESHAPER_DISTORTION,
    MODULATED_DELAY_LINE,
    BIQUAD_FILTER,
    DYNAMIC_RING_BUFFER,
    PITCH_SHIFTER_GRANULAR,
    ENVELOPE_VCA_COMPRESSOR,
    PITCH_DETECTOR_OSCILLATOR,
    DIFFUSED_DELAY_NETWORK,
    ALLPASS_FILTER_CASCADE,
    FREQUENCY_SHIFTER,
    MATHEMATICAL_WAVEFOLDER,
    SAMPLE_RATE_DEGRADER,
    FORMANT_VOCAL_SHIFTER,
    TAPE_STOP_REVERSE_ECHO,
    SIMPLE_DELAY,
    PLATE_REVERB,
    SOFT_DISTORTION,
    GRANULAR_DELAY
};

// Parameter token identifiers used across the system.
namespace ParamToken {
    constexpr uint16_t Wet    = 0;
    constexpr uint16_t Dry    = 1;
    constexpr uint16_t Volume = 2;
    constexpr uint16_t Effect = 3;
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
inline float calculatePixelAccumulation(const uint8_t* gridData,
                                         const PedalRowRange& range,
                                         int parameterIndex,
                                         int totalParameters)
{
    if (range.numRows == 0 || totalParameters == 0)
        return 0.0f;

    int cellsPerParam = (GridSize * range.numRows) / totalParameters;
    int startCell = parameterIndex * cellsPerParam;
    int endCell = (parameterIndex == totalParameters - 1)
                      ? (GridSize * range.numRows)
                      : (startCell + cellsPerParam);

    float accumulator = 0.0f;
    float aMin = 0.0f;
    float aMax = 0.0f;

    for (int localCell = startCell; localCell < endCell; ++localCell)
    {
        int gridX = localCell % GridSize;
        int gridY = range.startRow + (localCell / GridSize);
        if (gridY >= GridSize) break;

        uint8_t val = gridData[gridY * GridSize + gridX];
        float contribution = signedPixelContribution(val, localCell);
        accumulator += contribution;

        aMax += 2.0f;
        aMin += -2.0f;
    }

    if (aMax == aMin) return 0.0f;
    float normalized = (accumulator - aMin) / (aMax - aMin);
    return (normalized < 0.0f) ? 0.0f : (normalized > 1.0f ? 1.0f : normalized);
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
    std::vector<float> apBuf[2];
    size_t apPtr[2];
    float decorrL;
    float decorrR;
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
    float feedback = static_cast<float>(config.feedbackBase + decayNormalised * config.feedbackRange);
    float monoIn = (dryL + dryR) * 0.5f;

    float combOut = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        size_t bufLen = state.combBuf[i].size();
        if (bufLen == 0) continue;

        float& bufVal = state.combBuf[i][state.combPtr[i]];
        float tap = bufVal;
        bufVal = monoIn + tap * feedback * config.combGains[i];
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
    int grainLen;
};

// Prepare a granular delay line.
inline void prepareGranularProcessor(GranularProcessorState& state,
                                      double sampleRate, double durationSec)
{
    double safeDuration = std::min(durationSec, MaxGranularDurationSec);
    size_t size = static_cast<size_t>(sampleRate * safeDuration);
    state.delayBuf.assign(size, 0.0f);
    state.writePtr = 0;
    state.readPtr = 0.0f;
    state.grainLen = 0;
}

// Reset a granular delay line to silence.
inline void resetGranularProcessor(GranularProcessorState& state)
{
    std::fill(state.delayBuf.begin(), state.delayBuf.end(), 0.0f);
    state.writePtr = 0;
    state.readPtr = 0.0f;
    state.grainLen = 0;
}

// Process one sample through granular pitch shifting / delay.
// sampleRate used for grain-length calculation when state.grainLen == 0.
inline float processGranularSample(float input, GranularProcessorState& state,
                                    float pitchRatio, double sampleRate,
                                    float grainDurationSec)
{
    size_t bufSize = state.delayBuf.size();
    if (bufSize == 0) return 0.0f;

    if (state.grainLen == 0 || state.readPtr >= static_cast<float>(state.grainLen))
        state.grainLen = static_cast<int>(sampleRate * grainDurationSec);

    state.delayBuf[state.writePtr] = input;

    float readPos = state.readPtr;
    size_t readIdx = static_cast<size_t>(readPos) % bufSize;
    float frac = readPos - std::floor(readPos);
    size_t nextIdx = (readIdx + 1) % bufSize;
    float sample = state.delayBuf[readIdx] * (1.0f - frac) + state.delayBuf[nextIdx] * frac;

    float window = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * (readPos / static_cast<float>(state.grainLen))));
    float out = sample * window;

    state.readPtr += pitchRatio;
    if (state.readPtr >= static_cast<float>(state.grainLen))
        state.readPtr -= static_cast<float>(state.grainLen);

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

// Ring buffer with read-head state.
struct RingBufferState
{
    std::vector<float> buf;
    size_t writePtr;
    float readHead;
};

inline void prepareRingBuffer(RingBufferState& state, double sampleRate, double durationSec)
{
    double safeDuration = std::min(durationSec, static_cast<double>(MaxRingBufferSec));
    size_t size = static_cast<size_t>(sampleRate * safeDuration);
    state.buf.assign(size, 0.0f);
    state.writePtr = 0;
    state.readHead = 0.0f;
}

inline void resetRingBuffer(RingBufferState& state)
{
    std::fill(state.buf.begin(), state.buf.end(), 0.0f);
    state.writePtr = 0;
    state.readHead = 0.0f;
}
