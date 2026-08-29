#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Core/DrawdioConstants.h"

inline float interpolateDelayRead(const std::vector<float>& buf, float pos)
{
    const size_t n = buf.size();
    if (n == 0)
        return 0.0f;

    pos = std::fmod(pos, static_cast<float>(n));
    if (pos < 0.0f)
        pos += static_cast<float>(n);

    if (n < 4)
    {
        size_t idx = static_cast<size_t>(pos) % n;
        float frac = pos - std::floor(pos);
        size_t next = (idx + 1) % n;
        float result = buf[idx] * (1.0f - frac) + buf[next] * frac;
        if (!std::isfinite(result))
            result = 0.0f;
        return result;
    }

    size_t idx0 = (static_cast<size_t>(pos) + n - 1) % n;
    size_t idx1 = static_cast<size_t>(pos) % n;
    size_t idx2 = (static_cast<size_t>(pos) + 1) % n;
    size_t idx3 = (static_cast<size_t>(pos) + 2) % n;
    float y0 = buf[idx0], y1 = buf[idx1], y2 = buf[idx2], y3 = buf[idx3];
    float mu = pos - std::floor(pos);
    float mu2 = mu * mu;
    float a0 = y3 - y2 - y0 + y1;
    float a1 = y0 - y1 - a0;
    float a2 = y2 - y0;
    float a3 = y1;
    float result = ((a0 * mu + a1) * mu2 + a2 * mu + a3);
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
