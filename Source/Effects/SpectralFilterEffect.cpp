#include <JuceHeader.h>
#include "Effects/SpectralFilterEffect.h"
#include <algorithm>
#include <cmath>

void SpectralFilterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_bqZ1.assign(static_cast<size_t>(numChannels), 0.0f);
    m_bqZ2.assign(static_cast<size_t>(numChannels), 0.0f);
}

void SpectralFilterEffect::reset()
{
    std::fill(m_bqZ1.begin(), m_bqZ1.end(), 0.0f);
    std::fill(m_bqZ2.begin(), m_bqZ2.end(), 0.0f);
}

void SpectralFilterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float center = effectParam;
    float freqHz = 100.0f + center * 8000.0f;
    float bwHz = 50.0f + center * 2000.0f;
    float R = 1.0f - 3.0f * bwHz / static_cast<float>(m_sampleRate);
    if (R < 0.0f) R = 0.0f;
    float theta = 2.0f * 3.14159265f * freqHz / static_cast<float>(m_sampleRate);
    float cosTheta = std::cos(theta);
    float b0 = (1.0f - R * R) * 0.5f;
    float a1 = -2.0f * R * cosTheta;
    float a2 = R * R;

    int chCount = std::min(c, static_cast<int>(m_bqZ1.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float x = b[ch][s];
        float& z1 = m_bqZ1[static_cast<size_t>(ch)];
        float& z2 = m_bqZ2[static_cast<size_t>(ch)];
        float y = b0 * x + z1;
        z1 = -a1 * y + z2;
        z2 = b0 * x - a2 * y;
        b[ch][s] = y;
    }
}

void SpectralFilterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float center = params[1];
    float width = params[0];
    float q = params[2];

    m_lfoPhase += 0.0006f;
    if (m_lfoPhase > 6.2831853f) m_lfoPhase -= 6.2831853f;
    center += std::sin(m_lfoPhase) * 0.10f;
    center = std::max(0.0f, std::min(1.0f, center));

    float freqHz = 100.0f + center * 8000.0f;
    float bwHz = 20.0f + width * 4000.0f;
    float Q = 0.5f + q * 9.5f;
    float R = 1.0f - 3.14159265f * bwHz / (Q * static_cast<float>(m_sampleRate));
    if (R < 0.0f) R = 0.0f;
    float theta = 2.0f * 3.14159265f * freqHz / static_cast<float>(m_sampleRate);
    float cosTheta = std::cos(theta);
    float b0 = (1.0f - R * R) * 0.5f;
    float a1 = -2.0f * R * cosTheta;
    float a2 = R * R;

    int chCount = std::min(c, static_cast<int>(m_bqZ1.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float& z1 = m_bqZ1[static_cast<size_t>(ch)];
        float& z2 = m_bqZ2[static_cast<size_t>(ch)];
        for (int s = 0; s < n; ++s)
        {
            float x = b[ch][s];
            float y = b0 * x + z1;
            z1 = -a1 * y + z2;
            z2 = b0 * x - a2 * y;
            b[ch][s] = y;
        }
    }
}
