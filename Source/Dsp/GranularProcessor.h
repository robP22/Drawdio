#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Core/DrawdioConstants.h"
#include "Dsp/DelayPrimitives.h"

struct GranularProcessorState
{
    std::vector<float> delayBuf;
    size_t writePtr = 0;
    float readPtrA = 0.0f;
    float readPtrB = 0.0f;
    int grainLen = 0;
    size_t grainBaseA = 0;
    size_t grainBaseB = 0;
    std::vector<float> window;
    int prevGrainLen = 0;
    uint32_t rngA = 12345;
    uint32_t rngB = 54321;
};

inline void prepareGranularProcessor(GranularProcessorState& state,
                                     double sampleRate,
                                     double durationSec)
{
    double safeDuration = std::min(durationSec, MaxGranularDurationSec);
    size_t size = static_cast<size_t>(sampleRate * safeDuration);
    state.delayBuf.assign(size, 0.0f);
    const size_t maxGrainLen = static_cast<size_t>(sampleRate * 0.5) + 1;
    state.window.assign(std::max<size_t>(1, std::min(size, maxGrainLen)), 0.0f);
    state.writePtr = 0;
    state.readPtrA = 0.0f;
    state.readPtrB = 0.0f;
    state.grainLen = 0;
    state.grainBaseA = 0;
    state.grainBaseB = 0;
    state.prevGrainLen = 0;
}

inline void resetGranularProcessor(GranularProcessorState& state)
{
    std::fill(state.delayBuf.begin(), state.delayBuf.end(), 0.0f);
    state.writePtr = 0;
    state.readPtrA = 0.0f;
    state.readPtrB = 0.0f;
    state.grainLen = 0;
    state.grainBaseA = 0;
    state.grainBaseB = 0;
    state.prevGrainLen = 0;
}

namespace
{
inline float granularSpray(uint32_t& rng)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (static_cast<float>(rng & 0x3FFF) / 16383.0f - 0.5f) * 0.20f;
}

// Selects a grain read window that is guaranteed to stay strictly behind the
// write head for the whole grain lifetime. A grain lasts up to
// grainLen / playbackSpeed samples, and the write head laps the window start
// after (bufSize - grainLen - offset) samples, so the offset is capped at
// bufSize - grainLen * max(1, 1/speed). At speed < 1 the read head is slower
// than the write head, hence the tighter bound; at speed >= 1 the read outruns
// the write head and the plain grain-length margin suffices.
inline size_t computeGrainBase(size_t writePtr, size_t bufSize, int grainLen,
                               float grainPosition, float spray, float playbackSpeed)
{
    constexpr float kMargin = 128.0f;
    const float speed = std::max(0.01f, playbackSpeed);
    float offset = (grainPosition + spray) * static_cast<float>(bufSize);
    float maxOffset = static_cast<float>(bufSize)
                    - static_cast<float>(grainLen) * std::max(1.0f, 1.0f / speed)
                    - kMargin;
    if (maxOffset < kMargin)
        maxOffset = kMargin;
    offset = std::max(kMargin, std::min(maxOffset, offset));

    int64_t base = static_cast<int64_t>(writePtr) - static_cast<int64_t>(grainLen)
                 - static_cast<int64_t>(offset + 0.5f);
    base %= static_cast<int64_t>(bufSize);
    if (base < 0)
        base += static_cast<int64_t>(bufSize);
    return static_cast<size_t>(base);
}

inline void rebuildGrainWindow(GranularProcessorState& state)
{
    size_t wLen = std::min(static_cast<size_t>(state.grainLen), state.window.size());
    if (wLen == 0)
        return;
    if (wLen == 1)
    {
        state.window[0] = 1.0f;
        state.prevGrainLen = state.grainLen;
        return;
    }
    for (size_t wi = 0; wi < wLen; ++wi)
    {
        float phase = static_cast<float>(wi) / static_cast<float>(state.grainLen);
        state.window[wi] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * phase));
    }
    state.prevGrainLen = state.grainLen;
}
}

// Dual-grain crossfade: two independent grain heads with restart events
// staggered by half a grain length. Each head's Hann window is zero at its own
// restart, so the summed window stays continuous (unity for even grain
// lengths) and grain restarts are click-free.
inline float processGranularSample(float input, GranularProcessorState& state,
                                   float playbackSpeed,
                                   double sampleRate,
                                   float grainDurationSec,
                                   float grainPosition = 0.0f)
{
    size_t bufSize = state.delayBuf.size();
    if (bufSize == 0)
        return 0.0f;

    if (state.grainLen == 0)
    {
        state.grainLen = std::max(1, static_cast<int>(sampleRate * grainDurationSec));
        if (state.grainLen != state.prevGrainLen)
            rebuildGrainWindow(state);

        state.readPtrA = 0.0f;
        state.readPtrB = static_cast<float>(state.grainLen) * 0.5f;
        state.grainBaseA = computeGrainBase(state.writePtr, bufSize, state.grainLen,
                                            grainPosition, granularSpray(state.rngA),
                                            playbackSpeed);
        state.grainBaseB = computeGrainBase(state.writePtr, bufSize, state.grainLen,
                                            grainPosition, granularSpray(state.rngB),
                                            playbackSpeed);
    }

    if (!std::isfinite(input))
        input = 0.0f;
    state.delayBuf[state.writePtr] = input;

    float grainLenF = static_cast<float>(state.grainLen);
    float bufSizeF = static_cast<float>(bufSize);

    if (state.readPtrA >= grainLenF)
    {
        state.readPtrA -= grainLenF;
        state.grainBaseA = computeGrainBase(state.writePtr, bufSize, state.grainLen,
                                            grainPosition, granularSpray(state.rngA),
                                            playbackSpeed);
    }
    if (state.readPtrB >= grainLenF)
    {
        state.readPtrB -= grainLenF;
        state.grainBaseB = computeGrainBase(state.writePtr, bufSize, state.grainLen,
                                            grainPosition, granularSpray(state.rngB),
                                            playbackSpeed);
    }

    float posA = static_cast<float>(state.grainBaseA) + state.readPtrA;
    if (posA >= bufSizeF)
        posA -= bufSizeF;
    float posB = static_cast<float>(state.grainBaseB) + state.readPtrB;
    if (posB >= bufSizeF)
        posB -= bufSizeF;

    float s1 = interpolateDelayRead(state.delayBuf, posA);
    float s2 = interpolateDelayRead(state.delayBuf, posB);

    size_t wi1 = std::min(static_cast<size_t>(state.readPtrA),
                          static_cast<size_t>(state.grainLen) - 1);
    size_t wi2 = std::min(static_cast<size_t>(state.readPtrB),
                          static_cast<size_t>(state.grainLen) - 1);
    float w1 = state.window[wi1];
    float w2 = state.window[wi2];

    float out = s1 * w1 + s2 * w2;

    state.readPtrA += playbackSpeed;
    state.readPtrB += playbackSpeed;

    state.writePtr = (state.writePtr + 1) % bufSize;

    return out;
}
