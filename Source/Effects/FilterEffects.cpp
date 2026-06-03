#include "Effects/FilterEffects.h"

#include <algorithm>
#include <cmath>

void BiquadFilterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_lpState.assign(static_cast<size_t>(numChannels), 0.0f);
}

void BiquadFilterEffect::reset()
{
    std::fill(m_lpState.begin(), m_lpState.end(), 0.0f);
}

void BiquadFilterEffect::processSample(float** b, int c, int s, float effectParam)
{
    float freq = effectParam;
    float fc = 20.0f + (20000.0f - 20.0f) * freq * freq;
    float a = 1.0f - std::exp(-2.0f * 3.14159265f * fc / m_sampleRate);

    int maxCh = std::min(c, static_cast<int>(m_lpState.size()));
    for (int ch = 0; ch < maxCh; ++ch)
    {
        float x = b[ch][s];
        float& lp = m_lpState[static_cast<size_t>(ch)];
        lp = lp + a * (x - lp);
        b[ch][s] = lp;
    }
}

void AllpassCascadeEffect::prepare(double, int)
{
    m_delays.assign(2, std::vector<float>(4, 0.0f));
}

void AllpassCascadeEffect::reset()
{
    for (auto& ch : m_delays)
        std::fill(ch.begin(), ch.end(), 0.0f);
}

void AllpassCascadeEffect::processSample(float** b, int c, int s, float effectParam)
{
    float coeff = effectParam;
    float a = coeff * 0.97f;
    int nStages = 2;

    for (int ch = 0; ch < c && ch < 2; ++ch)
    {
        float x = b[ch][s];
        for (int stage = 0; stage < nStages && stage < 4; ++stage)
        {
            float& z = m_delays[static_cast<size_t>(ch)][static_cast<size_t>(stage)];
            float y = -a * x + z;
            z = x + a * y;
            x = y;
        }
        b[ch][s] = x;
    }
}

void FormantShifterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_lpState.assign(static_cast<size_t>(numChannels), 0.0f);
    m_envState = 0.0f;
}

void FormantShifterEffect::reset()
{
    std::fill(m_lpState.begin(), m_lpState.end(), 0.0f);
    m_envState = 0.0f;
}

void FormantShifterEffect::processSample(float** b, int c, int s, float effectParam)
{
    float formantFreq = effectParam;

    float attackCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.002)));
    float releaseCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.1)));

    float inputLevel = (c > 0) ? std::abs(b[0][s]) : 0.0f;
    if (inputLevel > m_envState)
        m_envState = attackCoeff * m_envState + (1.0f - attackCoeff) * inputLevel;
    else
        m_envState = releaseCoeff * m_envState + (1.0f - releaseCoeff) * inputLevel;

    float envMod = (m_envState > 0.5f) ? 1.0f : (m_envState / 0.5f);
    float centerHz = 200.0f + formantFreq * 1800.0f * (0.3f + envMod * 0.7f);

    float fc = centerHz / static_cast<float>(m_sampleRate);
    float bw = 0.1f;
    float R = 1.0f - bw * 3.14159265f * fc;
    float a = 1.0f - std::exp(-2.0f * 3.14159265f * fc * R);

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        float& lp = m_lpState[static_cast<size_t>(ch)];
        lp = lp + a * (x - lp);
        b[ch][s] = lp;
    }
}
