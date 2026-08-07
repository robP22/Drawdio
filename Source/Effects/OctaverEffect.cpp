#include <JuceHeader.h>
#include "Effects/OctaverEffect.h"

#include <algorithm>
#include <cmath>

namespace
{
void biquadLp(float sr, float cutoff, float& b0, float& b1, float& b2, float& a1, float& a2)
{
    const float w0 = 6.2831853f * cutoff / sr;
    const float cosw = std::cos(w0);
    const float alpha = std::sin(w0) * 0.70710678f;
    const float invA = 1.0f / (1.0f + alpha);
    b0 = (1.0f - cosw) * 0.5f * invA;
    b1 = (1.0f - cosw) * invA;
    b2 = b0;
    a1 = -2.0f * cosw * invA;
    a2 = (1.0f - alpha) * invA;
}

void biquadBp(float sr, float center, float q, float& b0, float& b1, float& b2, float& a1, float& a2)
{
    const float w0 = 6.2831853f * center / sr;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float invA = 1.0f / (1.0f + alpha);
    b0 = alpha * invA;
    b1 = 0.0f;
    b2 = -alpha * invA;
    a1 = -2.0f * std::cos(w0) * invA;
    a2 = (1.0f - alpha) * invA;
}
}

void OctaverEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.prevSign = 1.0f;
        ch.flipFlop = false;
        ch.dcZ1 = 0.0f;
        ch.dcPrevIn = 0.0f;
        ch.bpZ1 = 0.0f; ch.bpZ2 = 0.0f;
        ch.lpZ1 = 0.0f; ch.lpZ2 = 0.0f;
        ch.toneZ1 = 0.0f; ch.toneZ2 = 0.0f;
    }
    biquadBp(static_cast<float>(sampleRate), 400.0f, 1.5f,
             m_bpB0, m_bpB1, m_bpB2, m_bpA1, m_bpA2);
    biquadLp(static_cast<float>(sampleRate), 200.0f,
             m_lpB0, m_lpB1, m_lpB2, m_lpA1, m_lpA2);
    m_prevTone = -1.0f;
}

void OctaverEffect::reset()
{
    for (auto& ch : m_channels)
    {
        ch.prevSign = 1.0f;
        ch.flipFlop = false;
        ch.dcZ1 = 0.0f;
        ch.dcPrevIn = 0.0f;
        ch.bpZ1 = 0.0f; ch.bpZ2 = 0.0f;
        ch.lpZ1 = 0.0f; ch.lpZ2 = 0.0f;
        ch.toneZ1 = 0.0f; ch.toneZ2 = 0.0f;
    }
}

void OctaverEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float params[4] = {0.5f, effectParam, 0.5f, 0.5f};
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void OctaverEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float subLevel = std::max(0.0f, std::min(1.0f, params[1]));
    const float upperLevel = std::max(0.0f, std::min(1.0f, params[2]));
    const float toneParam = std::max(0.0f, std::min(1.0f, params[3]));

    const float sr = static_cast<float>(m_sampleRate);
    const float toneHz = 100.0f + toneParam * 1900.0f;
    if (std::abs(toneHz - m_prevTone) > std::abs(m_prevTone) * 0.05f || m_prevTone < 0.0f)
    {
        biquadLp(sr, toneHz, m_toneB0, m_toneB1, m_toneB2, m_toneA1, m_toneA2);
        m_prevTone = toneHz;
    }

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            if (!std::isfinite(in)) in = 0.0f;

            float sub = 0.0f;
            const float sign = (in >= 0.0f) ? 1.0f : -1.0f;
            if (sign != mc.prevSign && std::abs(in) > 0.005f)
                mc.flipFlop = !mc.flipFlop;
            mc.prevSign = sign;

            const float sq = mc.flipFlop ? 1.0f : -1.0f;
            const float lpSub = m_lpB0 * sq + mc.lpZ1;
            mc.lpZ1 = m_lpB1 * sq - m_lpA1 * lpSub + mc.lpZ2;
            mc.lpZ2 = m_lpB2 * sq - m_lpA2 * lpSub;
            sub = lpSub;

            const float bp = m_bpB0 * in + mc.bpZ1;
            mc.bpZ1 = m_bpB1 * in - m_bpA1 * bp + mc.bpZ2;
            mc.bpZ2 = m_bpB2 * in - m_bpA2 * bp;

            const float rect = std::abs(bp);
            const float dc = rect - mc.dcPrevIn + 0.9999f * mc.dcZ1;
            mc.dcZ1 = dc;
            mc.dcPrevIn = rect;

            float wet = sub * subLevel + dc * upperLevel;
            const float tone = m_toneB0 * wet + mc.toneZ1;
            mc.toneZ1 = m_toneB1 * wet - m_toneA1 * tone + mc.toneZ2;
            mc.toneZ2 = m_toneB2 * wet - m_toneA2 * tone;
            wet = tone;
            if (!std::isfinite(wet))
                wet = 0.0f;

            b[ch][s] = wet;
        }
    }
}
