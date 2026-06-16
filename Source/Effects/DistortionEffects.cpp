#include <JuceHeader.h>
#include "Effects/DistortionEffects.h"

#include <algorithm>
#include <cmath>

void WaveshaperEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float drive = effectParam;
    float clip = 0.5f + drive * 0.5f;
    if (drive < 0.01f) return;
    if (drive < 0.001f) drive = 0.001f;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        b[ch][s] = (2.0f / 3.14159265f) * std::atan(x * drive * 5.0f) * clip;
    }
}

void WaveshaperEffect::processBlock(float** b, int c, int n, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float drive = effectParam;
    float clip = 0.5f + drive * 0.5f;
    if (drive < 0.01f) return;
    if (drive < 0.001f) drive = 0.001f;

    for (int ch = 0; ch < c; ++ch)
        for (int s = 0; s < n; ++s)
            b[ch][s] = (2.0f / 3.14159265f) * std::atan(b[ch][s] * drive * 5.0f) * clip;
}

void WavefolderEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float d = 1.0f + effectParam * 4.0f;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        float folded = std::sin(x * d * 3.14159265f);
        float norm = 1.0f + d * 0.1f;
        b[ch][s] = folded / norm;
    }
}

void WavefolderEffect::processBlock(float** b, int c, int n, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float d = 1.0f + effectParam * 4.0f;
    float norm = 1.0f + d * 0.1f;

    for (int ch = 0; ch < c; ++ch)
        for (int s = 0; s < n; ++s)
            b[ch][s] = std::sin(b[ch][s] * d * 3.14159265f) / norm;
}

void CombResonatorEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_delays.resize(static_cast<size_t>(numChannels));
    for (auto& d : m_delays)
        prepareSimpleDelay(d, sampleRate, 0.5);
}

void CombResonatorEffect::reset()
{
    for (auto& d : m_delays)
        resetSimpleDelay(d);
}

void CombResonatorEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float freq = 20.0f * std::pow(66.666f, effectParam);
    float feedback = 0.85f;

    int chCount = std::min(c, static_cast<int>(m_delays.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_delays[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        size_t delaySamples = static_cast<size_t>(m_sampleRate / freq + 0.5f);
        if (delaySamples < 1) delaySamples = 1;
        if (delaySamples >= bufSize) delaySamples = bufSize - 1;

        float in = b[ch][s];
        size_t readPtr = (d.writePtr + bufSize - delaySamples) % bufSize;
        float delayed = d.buf[readPtr];
        d.buf[d.writePtr] = in + delayed * feedback;
        b[ch][s] = delayed;
        d.writePtr = (d.writePtr + 1) % bufSize;
    }
}
