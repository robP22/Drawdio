#include <JuceHeader.h>
#include "Effects/ConvolutionSpaceEffect.h"
#include <algorithm>
#include <cmath>
#include <random>

static std::vector<float> generateSyntheticIR(double sampleRate, float decaySec)
{
    size_t irLen = static_cast<size_t>(sampleRate * decaySec);
    if (irLen < 16) irLen = 16;
    if (irLen > 256) irLen = 256;

    std::vector<float> ir(irLen, 0.0f);
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    ir[0] = 1.0f;

    for (size_t i = 1; i < irLen; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(irLen);
        float env = std::exp(-t * 4.0f);
        ir[i] = dist(rng) * env * 0.15f;
    }

    float peak = 0.0f;
    for (auto& v : ir) peak = std::max(peak, std::abs(v));
    if (peak > 0.0f)
        for (auto& v : ir) v /= peak * 1.2f;

    return ir;
}

void ConvolutionSpaceEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    m_currentPreset = 0;

    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 1.0), 0.0f);
        ch.writePtr = 0;
        ch.ir = generateSyntheticIR(sampleRate, 0.8f);
        ch.irLen = ch.ir.size();
    }
}

void ConvolutionSpaceEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
    }
}

void ConvolutionSpaceEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float damp = effectParam;
    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& chState = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = chState.buf.size();
        if (bufSize == 0) continue;
        float inVal = b[ch][s];
        if (!std::isfinite(inVal)) inVal = 0.0f;
        chState.buf[chState.writePtr] = inVal;
        float out = 0.0f;
        float dampScale = damp * 0.9f / static_cast<float>(chState.irLen);
        for (size_t i = 0; i < chState.irLen && i < bufSize; ++i)
        {
            size_t idx = chState.writePtr >= i ? chState.writePtr - i : chState.writePtr + bufSize - i;
            out += chState.buf[idx] * chState.ir[i] * (1.0f - dampScale * static_cast<float>(i));
        }
        b[ch][s] = out;
        chState.writePtr = (chState.writePtr + 1) % bufSize;
    }
}

void ConvolutionSpaceEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float damp = params[3];
    float dampScale = damp * 0.9f;
    int chCount = std::min(c, static_cast<int>(m_channels.size()));

    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& chState = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = chState.buf.size();
        if (bufSize == 0) continue;

        float invIrLen = 1.0f / static_cast<float>(chState.irLen);
        float dampMul = dampScale * invIrLen;

        for (int s = 0; s < n; ++s)
        {
        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        chState.buf[chState.writePtr] = in;
            float out = 0.0f;
            size_t wp = chState.writePtr;

            for (size_t i = 0; i < chState.irLen && i < bufSize; ++i)
            {
                size_t idx = wp >= i ? wp - i : wp + bufSize - i;
                out += chState.buf[idx] * chState.ir[i] * (1.0f - dampMul * static_cast<float>(i));
            }

            b[ch][s] = out;
            chState.writePtr = (wp + 1) % bufSize;
        }
    }
}
