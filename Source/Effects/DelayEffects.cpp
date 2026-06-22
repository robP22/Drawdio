#include <JuceHeader.h>
#include "Effects/DelayEffects.h"

#include <algorithm>
#include <cmath>

void MicroPitchChorusEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 0.5), 0.0f);
        ch.writePtr = 0;
        ch.readPos1 = 0.0f;
        ch.readPos2 = 0.0f;
        ch.lfoPhase = 0.0f;
    }
}

void MicroPitchChorusEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.readPos1 = 0.0f;
        ch.readPos2 = 0.0f;
        ch.lfoPhase = 0.0f;
    }
}

void MicroPitchChorusEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float detuneCents = effectParam * 50.0f;
    float pitch1 = 1.0f + detuneCents / 1200.0f;
    float pitch2 = 1.0f - detuneCents / 1200.0f;

    static constexpr float kLfoRate = 0.3f;
    static constexpr float kLfoDepthSamples = 0.002f;  // 2ms

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = mc.buf.size();
        if (bufSize == 0) continue;

        mc.buf[mc.writePtr] = b[ch][s];

        mc.lfoPhase += static_cast<float>(kLfoRate / m_sampleRate);
        if (mc.lfoPhase >= 1.0f) mc.lfoPhase -= 1.0f;
        float lfo1 = std::sin(mc.lfoPhase * 2.0f * 3.14159265f);
        float lfo2 = std::sin((mc.lfoPhase + 0.5f) * 2.0f * 3.14159265f);

        float mod1 = lfo1 * kLfoDepthSamples * static_cast<float>(m_sampleRate);
        float mod2 = lfo2 * kLfoDepthSamples * static_cast<float>(m_sampleRate);

        float depthMod = m_depth * 2.0f;
        mc.readPos1 += pitch1 + mod1 * 0.001f * depthMod;
        if (mc.readPos1 >= static_cast<float>(bufSize))
            mc.readPos1 -= static_cast<float>(bufSize);
        else if (mc.readPos1 < 0.0f)
            mc.readPos1 += static_cast<float>(bufSize);

        mc.readPos2 += pitch2 + mod2 * 0.001f * depthMod;
        if (mc.readPos2 >= static_cast<float>(bufSize))
            mc.readPos2 -= static_cast<float>(bufSize);
        else if (mc.readPos2 < 0.0f)
            mc.readPos2 += static_cast<float>(bufSize);

        auto readTap = [&](float pos) -> float {
            return interpolateDelayRead(mc.buf, pos);
        };

        float tap1 = readTap(mc.readPos1);
        float tap2 = readTap(mc.readPos2);
        b[ch][s] = tap1 * 0.35f + tap2 * 0.35f;

        mc.writePtr = (mc.writePtr + 1) % bufSize;
    }
}

void SimpleDelayEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float delaySec = 0.1f + params[3] * 0.9f;
    float feedback = 0.3f + params[3] * 0.6f;

    int chCount = std::min(c, static_cast<int>(m_delays.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_delays[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        size_t delaySamples = static_cast<size_t>(m_sampleRate * delaySec);
        if (delaySamples >= bufSize) delaySamples = bufSize - 1;
        float& fbLp = m_fbLpState[static_cast<size_t>(ch)];

        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            size_t readPtr = (d.writePtr + bufSize - delaySamples) % bufSize;
            float delayed = d.buf[readPtr];
            fbLp = fbLp + m_fbLpCoeff * (delayed - fbLp);
            d.buf[d.writePtr] = in + std::tanh(fbLp * feedback);
            b[ch][s] = delayed;
            d.writePtr = (d.writePtr + 1) % bufSize;
        }
    }
}

void MicroPitchChorusEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    m_depth = params[1];
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[3]);
}

void SimpleDelayEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_delays.resize(static_cast<size_t>(numChannels));
    for (auto& d : m_delays)
        prepareSimpleDelay(d, sampleRate, 2.0);
    m_fbLpState.assign(static_cast<size_t>(numChannels), 0.0f);
    m_fbLpCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * 5000.0f / static_cast<float>(sampleRate));
}

void SimpleDelayEffect::reset()
{
    for (auto& d : m_delays)
        resetSimpleDelay(d);
    std::fill(m_fbLpState.begin(), m_fbLpState.end(), 0.0f);
}

void SimpleDelayEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float delaySec = 0.1f + effectParam * 0.9f;
    float feedback = 0.3f + effectParam * 0.6f;

    int chCount = std::min(c, static_cast<int>(m_delays.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_delays[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        size_t delaySamples = static_cast<size_t>(m_sampleRate * delaySec);
        if (delaySamples >= bufSize) delaySamples = bufSize - 1;

        float in = b[ch][s];
        size_t readPtr = (d.writePtr + bufSize - delaySamples) % bufSize;
        float delayed = d.buf[readPtr];
        float& fbLp = m_fbLpState[static_cast<size_t>(ch)];
        fbLp = fbLp + m_fbLpCoeff * (delayed - fbLp);
        d.buf[d.writePtr] = in + std::tanh(fbLp * feedback);
        b[ch][s] = delayed;
        d.writePtr = (d.writePtr + 1) % bufSize;
    }
}

void TapeStopEchoEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
        ch.writePtr = 0;
        ch.readSpeed = 1.0f;
        ch.readPos = static_cast<float>(ch.writePtr);
        ch.wasBraking = false;
    }
}

void TapeStopEchoEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.readPos = 0.0f;
        ch.readSpeed = 1.0f;
        ch.wasBraking = false;
    }
}

void TapeStopEchoEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float braking = effectParam;
    float brakeFactor = 0.98f - braking * 0.05f;
    float predelaySamps = m_predelayMs * 0.001f * static_cast<float>(m_sampleRate);

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& chState = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = chState.buf.size();
        if (bufSize == 0) continue;

        chState.buf[chState.writePtr] = b[ch][s];

        bool isBraking = (braking > 0.01f);
        if (isBraking && !chState.wasBraking)
        {
            chState.wasBraking = true;
            float pos = static_cast<float>(chState.writePtr) - predelaySamps;
            if (pos < 0.0f) pos += static_cast<float>(bufSize);
            chState.readPos = pos;
        }
        else if (!isBraking && chState.wasBraking)
        {
            chState.wasBraking = false;
            chState.readSpeed = 1.0f;
            chState.readPos = static_cast<float>(chState.writePtr);
        }

        if (isBraking)
            chState.readSpeed = std::fmax(0.001f, chState.readSpeed * brakeFactor + 0.001f);
        else
            chState.readSpeed = 1.0f;

        chState.readPos += chState.readSpeed;
        if (chState.readPos >= static_cast<float>(bufSize))
            chState.readPos -= static_cast<float>(bufSize);

        b[ch][s] = interpolateDelayRead(chState.buf, chState.readPos);

        chState.writePtr = (chState.writePtr + 1) % bufSize;
    }
}
