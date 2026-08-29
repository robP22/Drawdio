#include <JuceHeader.h>
#include "Effects/HpLpFilterEffect.h"
#include <algorithm>
#include <cmath>

void HpLpFilterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    m_prevHigh = 0.0f;
    m_prevLow = 1.0f;
    m_prevQ = 0.0f;
    reset();
}

void HpLpFilterEffect::reset()
{
    for (auto& ch : m_channels)
    {
        ch.hp = BiquadState{};
        ch.lp = BiquadState{};
    }
}

void HpLpFilterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    (void)effectParam;
    const float params[4] = {1.0f, 0.5f, 0.5f, 0.0f};
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void HpLpFilterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    if (n <= 0)
        return;

    const float sr = static_cast<float>(m_sampleRate);
    const float high0 = m_prevHigh;
    const float high1 = juce::jlimit(0.0f, 1.0f, params[1]);
    m_prevHigh = high1;
    const float low0 = m_prevLow;
    const float low1 = juce::jlimit(0.0f, 1.0f, params[2]);
    m_prevLow = low1;
    const float qp0 = m_prevQ;
    const float qp1 = juce::jlimit(0.0f, 1.0f, params[3]);
    m_prevQ = qp1;

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& st = m_channels[static_cast<size_t>(ch)];
        for (int s = 0; s < n; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(n);
            const float high = high0 + (high1 - high0) * t;
            const float low = low0 + (low1 - low0) * t;
            const float q = 0.5f + (qp0 + (qp1 - qp0) * t) * 1.5f;

            const float hpHz = 30.0f * std::pow(2000.0f / 30.0f, high);
            const float lpHz = 500.0f * std::pow(16000.0f / 500.0f, low);

            const float hpCos = std::cos(6.2831853f * hpHz / sr);
            const float lpCos = std::cos(6.2831853f * lpHz / sr);
            const float hpAlpha = std::sin(6.2831853f * hpHz / sr) / (2.0f * q);
            const float lpAlpha = std::sin(6.2831853f * lpHz / sr) / (2.0f * q);

            float hpB0, hpB1, hpB2, hpA1, hpA2;
            {
                const float invA = 1.0f / (1.0f + hpAlpha);
                hpB0 = (1.0f + hpCos) * 0.5f * invA;
                hpB1 = -(1.0f + hpCos) * invA;
                hpB2 = hpB0;
                hpA1 = -2.0f * hpCos * invA;
                hpA2 = (1.0f - hpAlpha) * invA;
            }
            float lpB0, lpB1, lpB2, lpA1, lpA2;
            {
                const float invA = 1.0f / (1.0f + lpAlpha);
                lpB0 = (1.0f - lpCos) * 0.5f * invA;
                lpB1 = (1.0f - lpCos) * invA;
                lpB2 = lpB0;
                lpA1 = -2.0f * lpCos * invA;
                lpA2 = (1.0f - lpAlpha) * invA;
            }

            float x = b[ch][s];
            if (!std::isfinite(x)) x = 0.0f;

            float v = x - hpA1 * st.hp.z1 - hpA2 * st.hp.z2;
            float y = hpB0 * v + hpB1 * st.hp.z1 + hpB2 * st.hp.z2;
            st.hp.z2 = st.hp.z1;
            st.hp.z1 = v;
            x = y;

            v = x - lpA1 * st.lp.z1 - lpA2 * st.lp.z2;
            y = lpB0 * v + lpB1 * st.lp.z1 + lpB2 * st.lp.z2;
            st.lp.z2 = st.lp.z1;
            st.lp.z1 = v;

            if (!std::isfinite(y))
            {
                y = 0.0f;
                st.hp = BiquadState{};
                st.lp = BiquadState{};
            }
            b[ch][s] = y;
        }
    }
}
