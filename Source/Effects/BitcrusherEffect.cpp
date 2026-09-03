#include <JuceHeader.h>
#include "Effects/BitcrusherEffect.h"

#include <algorithm>
#include <cmath>

namespace
{
inline float resamplerNoise(uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / 16777215.0f * 2.0f - 1.0f;
}
}

void BitcrusherEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (size_t i = 0; i < m_channels.size(); ++i)
    {
        m_channels[i] = BitcrusherChannel{};
        m_channels[i].rngState = static_cast<uint32_t>(0x9E3779B9u + i * 0x85EBCA6Bu);
    }
    m_prevCutoff = -1.0f;
}

void BitcrusherEffect::reset()
{
    for (auto& ch : m_channels)
    {
        ch.phase = 0.0;
        ch.hold = 0.0f;
        ch.prevInput = 0.0f;
        ch.lpZ1 = 0.0f;
        ch.lpZ2 = 0.0f;
        ch.dcZ1 = 0.0f;
        ch.dcPrevIn = 0.0f;
    }
}

void BitcrusherEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float params[4] = {1.0f, effectParam, 0.3f, 0.5f};
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void BitcrusherEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float rateParam = std::max(0.0f, std::min(1.0f, params[1]));
    const float bitsParam = std::max(0.0f, std::min(1.0f, params[2]));
    const float filterParam = std::max(0.0f, std::min(1.0f, params[3]));

    const float sr = static_cast<float>(m_sampleRate);
    const float targetRate = std::max(50.0f, sr * std::pow(500.0f / sr, rateParam));
    const float delta = targetRate / sr;
    const float levels = std::pow(2.0f, 2.0f + bitsParam * 14.0f);

    float cutoff = targetRate * 0.45f * (0.1f + 0.9f * filterParam);
    cutoff = std::max(20.0f, std::min(cutoff, sr * 0.49f));
    if (std::abs(cutoff - m_prevCutoff) > std::abs(m_prevCutoff) * 0.05f || m_prevCutoff < 0.0f)
    {
        m_lpB0Prev = m_lpB0; m_lpB1Prev = m_lpB1; m_lpB2Prev = m_lpB2;
        m_lpA1Prev = m_lpA1; m_lpA2Prev = m_lpA2;
        const float w0 = 6.2831853f * cutoff / sr;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) * 0.70710678f;
        const float invA = 1.0f / (1.0f + alpha);
        m_lpB0 = (1.0f - cosw) * 0.5f * invA;
        m_lpB1 = (1.0f - cosw) * invA;
        m_lpB2 = m_lpB0;
        m_lpA1 = -2.0f * cosw * invA;
        m_lpA2 = (1.0f - alpha) * invA;
        m_prevCutoff = cutoff;
    }

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        bool firstSample = true;
        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            if (!std::isfinite(in)) in = 0.0f;

            const float dc = in - mc.dcPrevIn + 0.9999f * mc.dcZ1;
            mc.dcZ1 = dc;
            mc.dcPrevIn = in;
            in = dc;

            const float it = static_cast<float>(s) / static_cast<float>(n);
            const float b0 = m_lpB0Prev + (m_lpB0 - m_lpB0Prev) * it;
            const float b1 = m_lpB1Prev + (m_lpB1 - m_lpB1Prev) * it;
            const float b2 = m_lpB2Prev + (m_lpB2 - m_lpB2Prev) * it;
            const float a1 = m_lpA1Prev + (m_lpA1 - m_lpA1Prev) * it;
            const float a2 = m_lpA2Prev + (m_lpA2 - m_lpA2Prev) * it;

            const float lp = b0 * in + mc.lpZ1;
            mc.lpZ1 = b1 * in - a1 * lp + mc.lpZ2;
            mc.lpZ2 = b2 * in - a2 * lp;
            in = lp;

            mc.phase += delta;
            if (mc.phase >= 1.0)
            {
                mc.phase -= 1.0;
                const float t2 = 1.0f - static_cast<float>(mc.phase) / delta;
                mc.hold = mc.prevInput + (in - mc.prevInput) * t2;
            }
            if (firstSample && mc.hold == 0.0f && in != 0.0f)
                mc.hold = in;
            firstSample = false;
            mc.prevInput = in;

            float out = mc.hold;
            {
                const float step = 1.0f / levels;
                const float ditherScale = std::min(1.0f, std::abs(mc.hold) * 10.0f);
                out += (resamplerNoise(mc.rngState) + resamplerNoise(mc.rngState)) * step * 0.5f * ditherScale;
            }
            out = std::round(out * levels) / levels;

            b[ch][s] = out;
        }
    }
}
