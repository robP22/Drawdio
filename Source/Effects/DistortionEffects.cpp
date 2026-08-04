#include <JuceHeader.h>
#include "Effects/DistortionEffects.h"

#include <algorithm>
#include <cmath>

void WaveshaperEffect::prepare(double, int numChannels)
{
    m_prevSample.assign(static_cast<size_t>(numChannels), 0.0f);
}

void WaveshaperEffect::reset()
{
    std::fill(m_prevSample.begin(), m_prevSample.end(), 0.0f);
}

void WaveshaperEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float drive = effectParam;
    float clip = 1.0f - drive * 0.5f;
    if (drive < 0.01f) return;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        b[ch][s] = (2.0f / 3.14159265f) * std::atan(x * drive * 5.0f) * clip;
    }
}

void WaveshaperEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float drive = params[3];
    if (drive < 0.01f) return;
    float clip = 1.0f - drive * 0.5f;
    float k = (2.0f / 3.14159265f) * clip;
    float alpha = drive * 5.0f;

    int chCount = std::min(c, static_cast<int>(m_prevSample.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float prev = m_prevSample[static_cast<size_t>(ch)];
        for (int s = 0; s < n; ++s)
        {
            float x2 = b[ch][s];
            float x1 = prev;
            prev = x2;
            float dx = x2 - x1;
            if (std::abs(dx) > 1e-10f)
            {
                float ax2 = alpha * x2;
                float ax1 = alpha * x1;
                float F2 = x2 * std::atan(ax2) - std::log1p(ax2 * ax2) / (2.0f * alpha);
                float F1 = x1 * std::atan(ax1) - std::log1p(ax1 * ax1) / (2.0f * alpha);
                b[ch][s] = k * (F2 - F1) / dx;
            }
            else
            {
                b[ch][s] = k * std::atan(alpha * (x1 + x2) * 0.5f);
            }
        }
        m_prevSample[static_cast<size_t>(ch)] = prev;
    }
}

void WavefolderEffect::prepare(double, int numChannels)
{
    m_prevSample.assign(static_cast<size_t>(numChannels), 0.0f);
}

void WavefolderEffect::reset()
{
    std::fill(m_prevSample.begin(), m_prevSample.end(), 0.0f);
}

void WavefolderEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float d = 1.0f + effectParam * 4.0f;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        if (!std::isfinite(x)) x = 0.0f;
        float folded = std::sin(x * d * 3.14159265f);
        float norm = 1.0f + d * 0.1f;
        b[ch][s] = folded / norm;
    }
}

void WavefolderEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float d = 1.0f + params[3] * 4.0f;
    float norm = 1.0f + d * 0.1f;
    float a = d * 3.14159265f;

    int chCount = std::min(c, static_cast<int>(m_prevSample.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float prev = m_prevSample[static_cast<size_t>(ch)];
        for (int s = 0; s < n; ++s)
        {
            float x2 = b[ch][s];
            float x1 = prev;
            prev = x2;
            float dx = x2 - x1;
            if (std::abs(dx) > 1e-10f)
            {
                float F2 = -std::cos(x2 * a) / (a * norm);
                float F1 = -std::cos(x1 * a) / (a * norm);
                b[ch][s] = (F2 - F1) / dx;
            }
            else
            {
                b[ch][s] = std::sin((x1 + x2) * 0.5f * a) / norm;
            }
        }
        m_prevSample[static_cast<size_t>(ch)] = prev;
    }
}

void CombResonatorEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_delays.resize(static_cast<size_t>(numChannels));
    for (auto& d : m_delays)
        prepareSimpleDelay(d, sampleRate, 0.5);
    m_dampState.assign(static_cast<size_t>(numChannels), 0.0f);
    m_dampCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * 3000.0f / static_cast<float>(sampleRate));
}

void CombResonatorEffect::reset()
{
    for (auto& d : m_delays)
        resetSimpleDelay(d);
    std::fill(m_dampState.begin(), m_dampState.end(), 0.0f);
}

void CombResonatorEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float freq = 20.0f * std::pow(66.666f, effectParam);
    float feedback = 0.85f;

    size_t delaySamples = static_cast<size_t>(m_sampleRate / freq + 0.5f);
    if (delaySamples < 1) delaySamples = 1;

    int chCount = std::min(c, static_cast<int>(m_delays.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_delays[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        size_t chDelaySamples = std::min(delaySamples, bufSize - 1);

        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        size_t readPtr = (d.writePtr + bufSize - chDelaySamples) % bufSize;
        float delayed = d.buf[readPtr];
        float& damp = m_dampState[static_cast<size_t>(ch)];
        damp = damp + m_dampCoeff * (delayed - damp);
        d.buf[d.writePtr] = in + std::tanh(damp * feedback);
        b[ch][s] = delayed;
        d.writePtr = (d.writePtr + 1) % bufSize;
    }
}

void CombResonatorEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float freq = 20.0f * std::pow(66.666f, params[3]);
    float feedback = 0.85f;

    int chCount = std::min(c, static_cast<int>(m_delays.size()));

    size_t delaySamples = static_cast<size_t>(m_sampleRate / freq + 0.5f);
    if (delaySamples < 1) delaySamples = 1;

    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_delays[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        size_t chDelaySamples = std::min(delaySamples, bufSize - 1);

        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            if (!std::isfinite(in)) in = 0.0f;
            size_t readPtr = (d.writePtr + bufSize - chDelaySamples) % bufSize;
            float delayed = d.buf[readPtr];
            float& damp = m_dampState[static_cast<size_t>(ch)];
            damp = damp + m_dampCoeff * (delayed - damp);
            d.buf[d.writePtr] = in + std::tanh(damp * feedback);
            b[ch][s] = delayed;
            d.writePtr = (d.writePtr + 1) % bufSize;
        }
    }
}
