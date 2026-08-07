#include "Dsp/ReverbNetwork.h"

#include <algorithm>
#include <cmath>

#include "Core/DrawdioConstants.h"
#include "Dsp/DelayPrimitives.h"

void prepareReverbNetwork(ReverbNetworkState& state, double sampleRate,
                          const ReverbNetworkConfig& config)
{
    for (int i = 0; i < 8; ++i)
    {
        size_t len = static_cast<size_t>(config.fdnTimesMs[static_cast<size_t>(i)])
                   * static_cast<size_t>(sampleRate) / 44100;
        if (len == 0) len = 1;
        state.fdnBuf[i].assign(len, 0.0f);
        state.fdnPtr[i] = 0;
    }
    for (int i = 0; i < 8; ++i)
        state.fdnDampState[i] = 0.0f;
    for (int i = 0; i < 8; ++i)
        state.fdnLfoPhase[i] = static_cast<float>(i) * 0.78539816f;
    for (int i = 0; i < 2; ++i)
        state.reflectBuf[i].assign(3072, 0.0f);
    state.reflectPtr = 0;
    state.decorrL = 0.0f;
    state.decorrR = 0.0f;
}

void resetReverbNetwork(ReverbNetworkState& state)
{
    for (int i = 0; i < 8; ++i)
    {
        std::fill(state.fdnBuf[i].begin(), state.fdnBuf[i].end(), 0.0f);
        state.fdnPtr[i] = 0;
    }
    for (int i = 0; i < 8; ++i)
        state.fdnDampState[i] = 0.0f;
    for (int i = 0; i < 2; ++i)
        std::fill(state.reflectBuf[i].begin(), state.reflectBuf[i].end(), 0.0f);
    state.reflectPtr = 0;
    state.decorrL = 0.0f;
    state.decorrR = 0.0f;
}

void processReverbNetworkSample(float dryL, float dryR,
                                const ReverbNetworkConfig& config,
                                ReverbNetworkState& state,
                                float decayNormalised,
                                float& outL, float& outR)
{
    if (!std::isfinite(dryL)) dryL = 0.0f;
    if (!std::isfinite(dryR)) dryR = 0.0f;

    float feedback = std::max(0.0f, static_cast<float>(config.feedbackBase + decayNormalised * config.feedbackRange));
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

    static constexpr float kModInc[8] = {
        0.0000869f, 0.000104f, 0.000124f, 0.000141f,
        0.000161f, 0.000183f, 0.000209f, 0.000231f
    };

    float tap[8];
    for (int i = 0; i < 8; ++i)
    {
        size_t bufLen = state.fdnBuf[i].size();
        if (bufLen == 0)
        {
            tap[i] = 0.0f;
            continue;
        }

        state.fdnLfoPhase[i] += kModInc[i];
        if (state.fdnLfoPhase[i] > 2.0f * 3.14159265f)
            state.fdnLfoPhase[i] -= 2.0f * 3.14159265f;

        float modPos = static_cast<float>(state.fdnPtr[i]) + std::sin(state.fdnLfoPhase[i]) * 50.0f;
        if (modPos < 0.0f) modPos += static_cast<float>(bufLen);
        tap[i] = interpolateDelayRead(state.fdnBuf[i], modPos);
        tap[i] = state.fdnDampState[i] = state.fdnDampState[i] + hfDamp * (tap[i] - state.fdnDampState[i]);
    }

    float sum = 0.0f;
    for (int i = 0; i < 8; ++i)
        sum += tap[i];
    sum *= 0.25f;

    for (int i = 0; i < 8; ++i)
    {
        size_t bufLen = state.fdnBuf[i].size();
        if (bufLen == 0)
            continue;

        float y = tap[i] - sum;
        state.fdnBuf[i][state.fdnPtr[i]] = monoIn + std::tanh(y * feedback * 0.98f) * 0.7f;
        state.fdnPtr[i] = (state.fdnPtr[i] + 1) % bufLen;
    }

    float combOut = 0.0f;
    for (int i = 0; i < 8; ++i)
        combOut += tap[i];
    combOut *= 0.125f;

    float k = 0.25f;
    outL = -k * combOut + state.decorrL;
    state.decorrL = combOut + k * outL;

    outR = -k * combOut + state.decorrR;
    state.decorrR = combOut + k * outR;
}
