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
    float detuneCents = effectParam * 50.0f;
    float pitch1 = 1.0f + detuneCents / 1200.0f;
    float pitch2 = 1.0f - detuneCents / 1200.0f;

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = mc.buf.size();
        if (bufSize == 0) continue;

        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        mc.buf[mc.writePtr] = in;

        mc.lfoPhase += m_lfoRate / static_cast<float>(m_sampleRate);
        if (mc.lfoPhase >= 1.0f) mc.lfoPhase -= 1.0f;
        float lfo1 = std::sin(mc.lfoPhase * 2.0f * 3.14159265f);
        float lfo2 = std::sin((mc.lfoPhase + 0.5f + static_cast<float>(ch) * 0.25f) * 2.0f * 3.14159265f);

        static constexpr float kLfoDepthSamples = 0.002f;
        float mod1 = lfo1 * kLfoDepthSamples * static_cast<float>(m_sampleRate);
        float mod2 = lfo2 * kLfoDepthSamples * static_cast<float>(m_sampleRate);

        float depthMod = m_depth * 2.0f;
        mc.readPos1 += pitch1 + mod1 * depthMod;
        if (mc.readPos1 >= static_cast<float>(bufSize))
            mc.readPos1 -= static_cast<float>(bufSize);
        else if (mc.readPos1 < 0.0f)
            mc.readPos1 += static_cast<float>(bufSize);

        mc.readPos2 += pitch2 + mod2 * depthMod;
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
    float delaySec = 0.1f + params[1] * 0.9f;
    float feedback = 0.3f + params[2] * 0.6f;
    float damp = params[3];
    float dampCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * (500.0f + damp * 15000.0f) / static_cast<float>(m_sampleRate));

    float targetDelay = static_cast<float>(m_sampleRate) * delaySec;
    float delayStart = m_smoothedDelaySamples;
    m_smoothedDelaySamples += (targetDelay - m_smoothedDelaySamples) * 0.05f;
    float delayEnd = m_smoothedDelaySamples;

    int chCount = std::min(c, static_cast<int>(m_delays.size()));
    float peak = 0.0f;
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_delays[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        float bufSizeF = static_cast<float>(bufSize);
        if (bufSize == 0) continue;

        float& fbLp = m_fbLpState[static_cast<size_t>(ch)];

        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            if (!std::isfinite(in)) in = 0.0f;

            const float t = static_cast<float>(s) / static_cast<float>(n);
            float delaySamplesF = delayStart + (delayEnd - delayStart) * t;
            if (delaySamplesF >= bufSizeF) delaySamplesF = bufSizeF - 1.0f;

            float readPos = static_cast<float>(d.writePtr + bufSize) - delaySamplesF;
            if (readPos >= bufSizeF) readPos -= bufSizeF;
            float delayed = interpolateDelayRead(d.buf, readPos);
            fbLp = fbLp + dampCoeff * (delayed - fbLp);
            d.buf[d.writePtr] = in + std::tanh(fbLp * feedback);
            b[ch][s] = delayed;
            peak = std::max(peak, std::abs(delayed));
            d.writePtr = (d.writePtr + 1) % bufSize;
        }
    }
    m_hasTail = (peak > 1e-8f);
}

void MicroPitchChorusEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    m_depth = params[1];
    m_lfoRate = 0.05f + params[3] * 2.95f;
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[2]);
}

void SimpleDelayEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_delays.resize(static_cast<size_t>(numChannels));
    for (auto& d : m_delays)
        prepareSimpleDelay(d, sampleRate, 2.0);
    m_fbLpState.assign(static_cast<size_t>(numChannels), 0.0f);
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

        float delaySamplesF = static_cast<float>(m_sampleRate) * delaySec;
        if (delaySamplesF >= static_cast<float>(bufSize)) delaySamplesF = static_cast<float>(bufSize) - 1.0f;

        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        float readPos = static_cast<float>(d.writePtr + bufSize) - delaySamplesF;
        if (readPos >= static_cast<float>(bufSize)) readPos -= static_cast<float>(bufSize);
        float delayed = interpolateDelayRead(d.buf, readPos);
        float& fbLp = m_fbLpState[static_cast<size_t>(ch)];
            fbLp = fbLp + 0.2f * (delayed - fbLp);
            d.buf[d.writePtr] = in + std::tanh(fbLp * feedback);
            b[ch][s] = delayed;
        d.writePtr = (d.writePtr + 1) % bufSize;
    }
}
