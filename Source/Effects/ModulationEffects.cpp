#include <JuceHeader.h>
#include "Effects/ModulationEffects.h"

#include <algorithm>
#include <cmath>

void ChorusEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_centerDelaySamples = static_cast<float>(sampleRate) * 0.030f;
    m_maxDepthSamples = static_cast<float>(sampleRate) * 0.006f;
    m_channels.resize(static_cast<size_t>(numChannels));
    for (size_t ch = 0; ch < m_channels.size(); ++ch)
    {
        m_channels[ch].buf.assign(static_cast<size_t>(sampleRate * 0.5), 0.0f);
        m_channels[ch].writePtr = 0;
        m_channels[ch].lfoPhase = static_cast<float>(ch) * 0.25f;
    }
}

void ChorusEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
    }
}

void ChorusEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float params[4] = {0.5f, effectParam, 0.5f, 0.5f};
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void ChorusEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float rateHz = 0.05f + std::max(0.0f, std::min(1.0f, params[3])) * 1.45f;
    const float depth = std::max(0.0f, std::min(1.0f, params[1]));
    float depthSamples = depth * m_maxDepthSamples;
    if (depthSamples >= m_centerDelaySamples - 1.0f)
        depthSamples = m_centerDelaySamples - 1.0f;
    const float sr = static_cast<float>(m_sampleRate);
    const float inc = rateHz / sr;

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        const size_t bufSize = mc.buf.size();
        const float bufSizeF = static_cast<float>(bufSize);
        if (bufSize == 0)
            continue;

        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            if (!std::isfinite(in)) in = 0.0f;
            mc.buf[mc.writePtr] = in;

            mc.lfoPhase += inc;
            if (mc.lfoPhase >= 1.0f) mc.lfoPhase -= 1.0f;
            const float lfo = std::sin(mc.lfoPhase * 6.2831853f);
            float readPos = static_cast<float>(mc.writePtr) - m_centerDelaySamples + lfo * depthSamples;
            if (readPos < 0.0f) readPos += bufSizeF;
            else if (readPos >= bufSizeF) readPos -= bufSizeF;

            b[ch][s] = interpolateDelayRead(mc.buf, readPos);
            mc.writePtr = (mc.writePtr + 1) % bufSize;
        }
    }
}

void TremoloEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (size_t ch = 0; ch < m_channels.size(); ++ch)
    {
        m_channels[ch].phase = static_cast<float>(ch) * 0.25f;
        m_channels[ch].squareSmooth = 0.0f;
    }
}

void TremoloEffect::reset()
{
    for (size_t ch = 0; ch < m_channels.size(); ++ch)
    {
        m_channels[ch].phase = static_cast<float>(ch) * 0.25f;
        m_channels[ch].squareSmooth = 0.0f;
    }
}

void TremoloEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float params[4] = {0.5f, effectParam, 0.7f, 0.0f};
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void TremoloEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float rateHz = 0.1f + params[1] * 19.9f;
    const float depth = std::max(0.0f, std::min(1.0f, params[2]));
    if (depth < 0.001f)
        return;

    const int shape = static_cast<int>(params[3] * 2.999f);
    const float sr = static_cast<float>(m_sampleRate);
    const float inc = rateHz / sr;
    const float sqCoeff = 1.0f - std::exp(-6.2831853f * 60.0f / sr);

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        for (int s = 0; s < n; ++s)
        {
            float lfo;
            switch (shape)
            {
                case 0:
                    lfo = 0.5f + 0.5f * std::sin(mc.phase * 6.2831853f);
                    break;
                case 1:
                    lfo = 1.0f - 2.0f * std::fabs(mc.phase - 0.5f);
                    break;
                default:
                {
                    const float raw = mc.phase < 0.5f ? 1.0f : 0.0f;
                    mc.squareSmooth += sqCoeff * (raw - mc.squareSmooth);
                    lfo = mc.squareSmooth;
                    break;
                }
            }
            const float gain = (1.0f - depth) + depth * lfo;
            float x = b[ch][s];
            if (!std::isfinite(x)) x = 0.0f;
            b[ch][s] = x * gain;

            mc.phase += inc;
            if (mc.phase >= 1.0f)
                mc.phase -= 1.0f;
        }
    }
}

void FlangerEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 0.015), 0.0f);
        ch.writePtr = 0;
        ch.lfoPhase = 0.0f;
    }
    if (m_channels.size() > 1)
        m_channels[1].lfoPhase = 0.5f;
    m_minDelaySamples = static_cast<float>(sampleRate) * 0.001f;
    m_maxDelaySamples = static_cast<float>(sampleRate) * 0.010f;
}

void FlangerEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.lfoPhase = 0.0f;
        ch.fbLp = 0.0f;
    }
    if (m_channels.size() > 1)
        m_channels[1].lfoPhase = 0.5f;
}

void FlangerEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float params[4] = {0.5f, effectParam, 0.5f, 0.3f};
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void FlangerEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float rateHz = 0.05f + params[1] * 1.45f;
    const float depth = std::max(0.0f, std::min(1.0f, params[2]));
    const float feedback = std::max(0.0f, std::min(0.7f, params[3]));
    const float sr = static_cast<float>(m_sampleRate);
    const float inc = rateHz / sr;
    const float depthSamples = depth * (m_maxDelaySamples - m_minDelaySamples);
    const float fbLpCoeff = 1.0f - std::exp(-6.2831853f * 4000.0f / sr);

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        const size_t bufSize = mc.buf.size();
        const float bufSizeF = static_cast<float>(bufSize);
        if (bufSize == 0)
            continue;

        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            if (!std::isfinite(in)) in = 0.0f;

            const float lfo = 0.5f + 0.5f * std::sin(mc.lfoPhase * 6.2831853f);
            float delaySamples = m_minDelaySamples + lfo * depthSamples;
            if (delaySamples >= bufSizeF)
                delaySamples = bufSizeF - 1.0f;

            float readPos = static_cast<float>(mc.writePtr + bufSize) - delaySamples;
            if (readPos >= bufSizeF)
                readPos -= bufSizeF;

            const float delayed = interpolateDelayRead(mc.buf, readPos);
            mc.fbLp += fbLpCoeff * (delayed - mc.fbLp);
            mc.buf[mc.writePtr] = in + std::tanh(mc.fbLp * feedback);
            b[ch][s] = delayed;
            mc.writePtr = (mc.writePtr + 1) % bufSize;

            mc.lfoPhase += inc;
            if (mc.lfoPhase >= 1.0f)
                mc.lfoPhase -= 1.0f;
        }
    }
}
