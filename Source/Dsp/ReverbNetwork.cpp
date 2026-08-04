#include "Dsp/ReverbNetwork.h"

#include <algorithm>
#include <cmath>

#include "Core/DrawdioConstants.h"
#include "Dsp/DelayPrimitives.h"

void prepareReverbNetwork(ReverbNetworkState& state, double sampleRate,
                          const ReverbNetworkConfig& config)
{
    for (int i = 0; i < 4; ++i)
    {
        size_t len = static_cast<size_t>(config.combTimesMs[i]) * static_cast<size_t>(sampleRate) / 44100;
        if (len == 0) len = 1;
        state.combBuf[i].assign(len, 0.0f);
        state.combPtr[i] = 0;
    }
    for (int i = 0; i < 2; ++i)
    {
        size_t len = static_cast<size_t>(config.apTimesMs[i]) * static_cast<size_t>(sampleRate) / 44100;
        if (len == 0) len = 1;
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

void resetReverbNetwork(ReverbNetworkState& state)
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

    float combOut = 0.0f;
    static constexpr float kModInc[4] = { 0.0000869f, 0.000124f, 0.000161f, 0.000209f };
    for (int i = 0; i < 4; ++i)
    {
        size_t bufLen = state.combBuf[i].size();
        if (bufLen == 0)
            continue;

        state.combLfoPhase[i] += kModInc[i];
        if (state.combLfoPhase[i] > 2.0f * 3.14159265f)
            state.combLfoPhase[i] -= 2.0f * 3.14159265f;

        float modPos = static_cast<float>(state.combPtr[i]) + std::sin(state.combLfoPhase[i]) * 50.0f;
        if (modPos < 0.0f) modPos += static_cast<float>(bufLen);
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
        if (bufLen == 0)
            continue;

        float& bufVal = state.apBuf[i][state.apPtr[i]];
        float tap = bufVal;
        bufVal = combOut + config.apCoeff * tap;
        combOut = -config.apCoeff * combOut + tap;
        state.apPtr[i] = (state.apPtr[i] + 1) % bufLen;
    }

    float k = 0.25f;
    outL = -k * combOut + state.decorrL;
    state.decorrL = combOut + k * outL;

    outR = -k * combOut + state.decorrR;
    state.decorrR = combOut + k * outR;
}
