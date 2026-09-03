#include <JuceHeader.h>
#include "Effects/DelayEffects.h"

#include <algorithm>
#include <cmath>

void DelayEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float delaySec = 0.1f + params[1] * 0.9f;
    float feedback = 0.3f + params[2] * 0.6f;
    float damp = params[3];
    float dampCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * (500.0f + damp * 15000.0f) / static_cast<float>(m_sampleRate));

    float targetDelay = static_cast<float>(m_sampleRate) * delaySec;
    if (m_firstBlock)
    {
        m_smoothedDelaySamples = targetDelay;
        m_firstBlock = false;
    }
    float delayStart = m_smoothedDelaySamples;
    m_smoothedDelaySamples += (targetDelay - m_smoothedDelaySamples) * 0.2f;
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

void DelayEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_delays.resize(static_cast<size_t>(numChannels));
    for (auto& d : m_delays)
        prepareSimpleDelay(d, sampleRate, 2.0);
    m_fbLpState.assign(static_cast<size_t>(numChannels), 0.0f);
    m_smoothedDelaySamples = static_cast<float>(sampleRate) * 0.55f;
}

void DelayEffect::reset()
{
    for (auto& d : m_delays)
        resetSimpleDelay(d);
    std::fill(m_fbLpState.begin(), m_fbLpState.end(), 0.0f);
    m_firstBlock = true;
}

void DelayEffect::processSample(float** b, int c, int s, float effectParam)
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

void GranularDelayEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float position = std::max(0.0f, std::min(1.0f, params[3]));
    const int chCount = std::min(c, static_cast<int>(m_states.size()));
    for (int ch = 0; ch < chCount; ++ch)
        for (int s = 0; s < n; ++s)
            b[ch][s] = processGranularSample(b[ch][s], m_states[static_cast<size_t>(ch)],
                                             1.0f, m_sampleRate, m_grainDurationSec, position);
}
