#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Core/DrawdioConstants.h"

inline float interpolateDelayRead(const std::vector<float>& buf, float pos)
{
    size_t idx = static_cast<size_t>(pos) % buf.size();
    float frac = pos - std::floor(pos);
    size_t next = (idx + 1) % buf.size();
    float result = buf[idx] * (1.0f - frac) + buf[next] * frac;
    if (!std::isfinite(result))
        result = 0.0f;
    return result;
}

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
