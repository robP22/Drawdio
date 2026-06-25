#include <JuceHeader.h>
#include "Effects/ReverseBufferEffect.h"
#include <algorithm>
#include <cmath>

void ReverseBufferEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_buffers.resize(static_cast<size_t>(numChannels));
    for (auto& d : m_buffers)
        prepareSimpleDelay(d, sampleRate, 1.5);

    m_prevWindowSamps = 0;
    m_xfadeCounter = kXfadeLen;
}

void ReverseBufferEffect::reset()
{
    for (auto& d : m_buffers)
        resetSimpleDelay(d);
    m_prevWindowSamps = 0;
    m_xfadeCounter = kXfadeLen;
}

void ReverseBufferEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float density = effectParam;
    float windowSec = 0.05f + density * 0.95f;

    int chCount = std::min(c, static_cast<int>(m_buffers.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_buffers[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        d.buf[d.writePtr] = in;

        size_t windowSamps = static_cast<size_t>(m_sampleRate * windowSec);
        if (windowSamps < 2) windowSamps = 2;
        if (windowSamps >= bufSize) windowSamps = bufSize - 1;

        size_t readIdx = (d.writePtr + bufSize - windowSamps) % bufSize;
        float out = d.buf[readIdx];

        b[ch][s] = out;
        d.writePtr = (d.writePtr + 1) % bufSize;
    }
}

void ReverseBufferEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float windowSec = 0.05f + params[1] * 0.95f;

    size_t windowSamps = static_cast<size_t>(m_sampleRate * windowSec);
    if (windowSamps < 2) windowSamps = 2;

    bool windowChanged = (windowSamps != m_prevWindowSamps);
    size_t oldSamps = m_prevWindowSamps;

    if (windowChanged)
    {
        if (m_xfadeCounter >= kXfadeLen)
            m_xfadeCounter = 0.0f;
        m_prevWindowSamps = windowSamps;
    }

    int chCount = std::min(c, static_cast<int>(m_buffers.size()));
    float xfCounter = m_xfadeCounter;

    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_buffers[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        size_t clampedSamps = windowSamps;
        if (clampedSamps >= bufSize) clampedSamps = bufSize - 1;
        size_t oldClampedSamps = oldSamps;
        if (oldClampedSamps >= bufSize) oldClampedSamps = bufSize - 1;

        for (int s = 0; s < n; ++s)
        {
            float fade = std::min(1.0f, xfCounter / kXfadeLen);
            d.buf[d.writePtr] = b[ch][s];

            if (xfCounter < kXfadeLen)
            {
                size_t oldRead = (d.writePtr + bufSize - oldClampedSamps) % bufSize;
                size_t newRead = (d.writePtr + bufSize - clampedSamps) % bufSize;
                b[ch][s] = d.buf[oldRead] * (1.0f - fade) + d.buf[newRead] * fade;
            }
            else
            {
                size_t readIdx = (d.writePtr + bufSize - clampedSamps) % bufSize;
                b[ch][s] = d.buf[readIdx];
            }

            d.writePtr = (d.writePtr + 1) % bufSize;

            if (ch == chCount - 1 && xfCounter < kXfadeLen)
                xfCounter += 1.0f;
        }
    }

    m_xfadeCounter = xfCounter;
}
