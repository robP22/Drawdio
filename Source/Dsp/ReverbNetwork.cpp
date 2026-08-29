#include "Dsp/ReverbNetwork.h"

#include <algorithm>
#include <cmath>

#include "Core/DrawdioConstants.h"
#include "Dsp/DelayPrimitives.h"

void prepareReverbNetwork(ReverbNetworkState& state, double sampleRate,
                          const ReverbNetworkConfig& config)
{
    const double srScale = sampleRate / 44100.0;

    for (int i = 0; i < 8; ++i)
    {
        size_t len = static_cast<size_t>(config.fdnTimes[static_cast<size_t>(i)])
                   * static_cast<size_t>(sampleRate) / 44100;
        if (len == 0) len = 1;
        state.fdnBuf[i].assign(len, 0.0f);
        state.fdnPtr[i] = 0;
    }
    for (int i = 0; i < 8; ++i)
        state.fdnDampState[i] = 0.0f;
    for (int i = 0; i < 8; ++i)
        state.fdnLfoPhase[i] = static_cast<float>(i) * 0.78539816f;
    for (int i = 0; i < 8; ++i)
        state.fdnDampCoeff[i] = 0.1f;
    for (int i = 0; i < 2; ++i)
        state.reflectBuf[i].assign(static_cast<size_t>(3072.0 * srScale + 0.5), 0.0f);
    state.reflectPtr = 0;
    state.decorrL = 0.0f;
    state.decorrR = 0.0f;

    double sum = 0.0;
    for (int i = 0; i < 8; ++i)
        sum += static_cast<double>(state.fdnBuf[static_cast<size_t>(i)].size());
    state.lineMean = static_cast<float>(sum / 8.0);
    if (state.lineMean < 1.0f)
        state.lineMean = 1.0f;

    static constexpr int kDiffDelays[3] = { 557, 613, 677 };
    for (int ch = 0; ch < 2; ++ch)
        for (int st = 0; st < 3; ++st)
        {
            auto& df = state.diffusers[static_cast<size_t>(ch)][static_cast<size_t>(st)];
            df.buf.assign(static_cast<size_t>(static_cast<double>(kDiffDelays[st]) * srScale + 0.5), 0.0f);
            df.writePtr = 0;
            df.phase = static_cast<float>(st) * 2.0943951f + static_cast<float>(ch) * 3.14159265f;
        }

    state.sampleRate = sampleRate;
}

void resetReverbNetwork(ReverbNetworkState& state)
{
    for (int i = 0; i < 8; ++i)
    {
        std::fill(state.fdnBuf[i].begin(), state.fdnBuf[i].end(), 0.0f);
        state.fdnPtr[i] = 0;
        state.fdnDampState[i] = 0.0f;
    }
    for (int i = 0; i < 2; ++i)
    {
        std::fill(state.reflectBuf[i].begin(), state.reflectBuf[i].end(), 0.0f);
        for (int st = 0; st < 3; ++st)
        {
            auto& df = state.diffusers[static_cast<size_t>(i)][static_cast<size_t>(st)];
            std::fill(df.buf.begin(), df.buf.end(), 0.0f);
            df.writePtr = 0;
        }
    }
    state.reflectPtr = 0;
    state.decorrL = 0.0f;
    state.decorrR = 0.0f;
}

void prepareReverbNetworkBlock(ReverbNetworkState& state,
                               const ReverbNetworkConfig& config,
                               float decayNormalised)
{
    (void)config;
    const float decay = std::max(0.0f, std::min(1.0f, decayNormalised));
    const float sr = static_cast<float>(state.sampleRate);
    const float baseFreq = std::max(1200.0f, 8000.0f * (1.0f - decay));
    for (int i = 0; i < 8; ++i)
    {
        float len = static_cast<float>(state.fdnBuf[static_cast<size_t>(i)].size());
        if (len < 1.0f) len = 1.0f;
        const float f = baseFreq * (state.lineMean / len);
        state.fdnDampCoeff[static_cast<size_t>(i)] = 1.0f - std::exp(-6.2831853f * f / sr);
    }
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

    static constexpr size_t kTapOffsets[5] = { 882, 1102, 1411, 1852, 2426 };
    static constexpr float kTapGains[5] = { 0.32f, 0.20f, 0.13f, 0.07f, 0.03f };

    const double srScale = state.sampleRate / 44100.0;

    size_t rLen = state.reflectBuf[0].size();
    state.reflectBuf[0][state.reflectPtr] = dryL;
    state.reflectBuf[1][state.reflectPtr] = dryR;

    float erL = 0.0f, erR = 0.0f;
    for (int t = 0; t < 5; ++t)
    {
        size_t tapOff = static_cast<size_t>(static_cast<double>(kTapOffsets[t]) * srScale + 0.5);
        size_t idx = (state.reflectPtr + rLen - tapOff) % rLen;
        erL += state.reflectBuf[0][idx] * kTapGains[t];
        erR += state.reflectBuf[1][idx] * kTapGains[t];
    }
    state.reflectPtr = (state.reflectPtr + 1) % rLen;

    static constexpr float kDiffCoeff = 0.6f;
    static constexpr float kDiffModInc[3] = { 0.000011f, 0.000016f, 0.000021f };
    static constexpr float kDiffModDepth = 12.0f;

    float diffusedL = 0.0f, diffusedR = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        float x = (ch == 0) ? erL : erR;
        for (int st = 0; st < 3; ++st)
        {
            auto& df = state.diffusers[static_cast<size_t>(ch)][static_cast<size_t>(st)];
            size_t bufLen = df.buf.size();
            if (bufLen == 0)
                continue;

            df.phase += static_cast<float>(kDiffModInc[st] * srScale);
            if (df.phase >= 6.2831853f)
                df.phase -= 6.2831853f;

            float delaySamples = static_cast<float>(bufLen) - 1.0f
                               - std::sin(df.phase) * kDiffModDepth;
            float readPos = static_cast<float>(df.writePtr + bufLen) - delaySamples;
            if (readPos >= static_cast<float>(bufLen))
                readPos -= static_cast<float>(bufLen);

            const float delayed = interpolateDelayRead(df.buf, readPos);
            const float y = -kDiffCoeff * x + delayed;
            df.buf[df.writePtr] = x + kDiffCoeff * y;
            df.writePtr = (df.writePtr + 1) % bufLen;
            x = y;
        }
        if (ch == 0)
            diffusedL = x;
        else
            diffusedR = x;
    }

    float monoIn = (diffusedL + diffusedR) * 0.5f;

    static constexpr float kModInc[8] = {
        0.0000869f, 0.000104f, 0.000124f, 0.000141f,
        0.000161f, 0.000183f, 0.000209f, 0.000231f
    };

    float tap[8];
    for (int i = 0; i < 8; ++i)
    {
        size_t bufLen = state.fdnBuf[static_cast<size_t>(i)].size();
        if (bufLen == 0)
        {
            tap[i] = 0.0f;
            continue;
        }

        state.fdnLfoPhase[i] += static_cast<float>(kModInc[i] * srScale);
        if (state.fdnLfoPhase[i] > 2.0f * 3.14159265f)
            state.fdnLfoPhase[i] -= 2.0f * 3.14159265f;

        float modPos = static_cast<float>(state.fdnPtr[i]) + std::sin(state.fdnLfoPhase[i]) * 50.0f;
        if (modPos < 0.0f) modPos += static_cast<float>(bufLen);
        tap[i] = interpolateDelayRead(state.fdnBuf[i], modPos);
        tap[i] = state.fdnDampState[i] = state.fdnDampState[i]
               + state.fdnDampCoeff[static_cast<size_t>(i)] * (tap[i] - state.fdnDampState[i]);
    }

    float sum = 0.0f;
    for (int i = 0; i < 8; ++i)
        sum += tap[i];

    for (int i = 0; i < 8; ++i)
    {
        size_t bufLen = state.fdnBuf[static_cast<size_t>(i)].size();
        if (bufLen == 0)
            continue;

        float y = tap[i] - sum * 0.25f;
        state.fdnBuf[i][state.fdnPtr[i]] = monoIn + y * feedback;
        state.fdnPtr[i] = (state.fdnPtr[i] + 1) % bufLen;
    }

    float combOut = 0.0f;
    for (int i = 0; i < 8; ++i)
        combOut += tap[i];
    combOut *= 0.125f;

    float k = 0.25f;
    float aL = -k * combOut + state.decorrL;
    state.decorrL = combOut + k * aL;
    float aR = -k * combOut + state.decorrR;
    state.decorrR = combOut + k * aR;

    const float wetScale = 1.0f - feedback;
    outL = aL * wetScale;
    outR = aR * wetScale;
}
