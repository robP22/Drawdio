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
    size_t writePtr;
    float readPtr;
    float grainPhase2;
    float grain2Pos;
    int grainLen;
    size_t grainBase;
    std::vector<float> window;
    int prevGrainLen = 0;
    uint32_t rngState = 12345;
};

inline void prepareGranularProcessor(GranularProcessorState& state,
                                     double sampleRate,
                                     double durationSec)
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
    state.prevGrainLen = 0;
}

inline void resetGranularProcessor(GranularProcessorState& state)
{
    std::fill(state.delayBuf.begin(), state.delayBuf.end(), 0.0f);
    state.writePtr = 0;
    state.readPtr = 0.0f;
    state.grainPhase2 = 0.0f;
    state.grain2Pos = 0.0f;
    state.grainLen = 0;
    state.grainBase = 0;
    state.prevGrainLen = 0;
}

inline float processGranularSample(float input, GranularProcessorState& state,
                                   float playbackSpeed,
                                   double sampleRate,
                                   float grainDurationSec,
                                   float grainPosition = 0.0f)
{
    size_t bufSize = state.delayBuf.size();
    if (bufSize == 0)
        return 0.0f;

    if (state.grainLen == 0 || state.readPtr >= static_cast<float>(state.grainLen))
    {
        state.grainLen = std::max(1, static_cast<int>(sampleRate * grainDurationSec));

        state.readPtr = 0.0f;

        state.rngState ^= state.rngState << 13;
        state.rngState ^= state.rngState >> 17;
        state.rngState ^= state.rngState << 5;
        float spray = (static_cast<float>(state.rngState & 0x3FFF) / 16383.0f - 0.5f) * 0.20f;

        float offset = (grainPosition + spray) * static_cast<float>(bufSize);
        while (offset < 0.0f) offset += static_cast<float>(bufSize);
        size_t off = static_cast<size_t>(offset) % bufSize;
        state.grainBase = (state.writePtr + bufSize - static_cast<size_t>(state.grainLen) - off) % bufSize;

        float grainLenF = static_cast<float>(state.grainLen);
        state.grainPhase2 = grainLenF * (0.5f + spray);
        state.grainPhase2 = std::max(0.05f * grainLenF, std::min(0.95f * grainLenF, state.grainPhase2));
        state.grain2Pos = static_cast<float>(state.grainBase) + state.grainPhase2;
        if (state.grain2Pos >= static_cast<float>(bufSize))
            state.grain2Pos -= static_cast<float>(bufSize);

        if (state.grainLen != state.prevGrainLen)
        {
            size_t wLen = std::min(static_cast<size_t>(state.grainLen), state.window.size());
            for (size_t wi = 0; wi < wLen; ++wi)
            {
                float phase = static_cast<float>(wi) / static_cast<float>(state.grainLen);
                state.window[wi] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * phase));
            }
            state.prevGrainLen = state.grainLen;
        }
    }

    if (!std::isfinite(input))
        input = 0.0f;
    state.delayBuf[state.writePtr] = input;

    float grainLenF = static_cast<float>(state.grainLen);
    float bufSizeF = static_cast<float>(bufSize);

    float pos1 = static_cast<float>(state.grainBase) + state.readPtr;
    if (pos1 >= bufSizeF)
        pos1 -= bufSizeF;

    float pos2 = state.grain2Pos;
    if (pos2 >= bufSizeF)
        pos2 -= bufSizeF;

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

    state.grain2Pos = static_cast<float>(state.grainBase) + state.grainPhase2;
    if (state.grain2Pos >= bufSizeF)
        state.grain2Pos -= bufSizeF;

    state.writePtr = (state.writePtr + 1) % bufSize;

    return out;
}
