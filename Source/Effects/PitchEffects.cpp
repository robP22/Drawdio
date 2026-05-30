#include "Effects/PitchEffects.h"

void FrequencyShifterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_phase = 0.0f;
    m_allpassZ[0] = 0.0f;
    m_allpassZ[1] = 0.0f;
}

void FrequencyShifterEffect::reset()
{
    m_phase = 0.0f;
    m_allpassZ[0] = 0.0f;
    m_allpassZ[1] = 0.0f;
}

void FrequencyShifterEffect::processSample(float** b, int c, int s, float effectParam)
{
    float shift = effectParam;
    float shiftHz = shift * shift * 2000.0f;

    m_phase += static_cast<float>(shiftHz / m_sampleRate);
    if (m_phase >= 1.0f) m_phase -= 1.0f;

    float cosPhi = std::cos(m_phase * 2.0f * 3.14159265f);
    float sinPhi = std::sin(m_phase * 2.0f * 3.14159265f);

    float w = static_cast<float>(3.14159265f * (300.0f + shiftHz * 0.3f) / m_sampleRate);
    float tanHalf = std::tan(w);
    float a = (tanHalf - 1.0f) / (tanHalf + 1.0f);

    for (int ch = 0; ch < c && ch < 2; ++ch)
    {
        float x = b[ch][s];
        float& z = m_allpassZ[ch];
        float q = a * x + z;
        z = x - a * q;

        float shifted = x * cosPhi + q * sinPhi;
        b[ch][s] = shifted;
    }
}

void SubSynthEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    reset();
}

void SubSynthEffect::reset()
{
    m_phase = 0.0f;
    m_prevSample = 0.0f;
    m_zeroCount = 0;
    m_measuredFreq = 100.0f;
    m_silenceCounter = 0;
}

void SubSynthEffect::processSample(float** b, int c, int s, float effectParam)
{
    float octave = effectParam;
    int octDiv = (octave < 0.5f) ? 2 : 4;

    float x0 = (c > 0) ? b[0][s] : 0.0f;

    // Input gate: track silence, freeze oscillator when no signal.
    float peak = std::abs(x0);
    if (peak < 0.001f)
        m_silenceCounter = std::min(m_silenceCounter + 1, kGateSamples);
    else
        m_silenceCounter = 0;

    if ((m_prevSample <= 0.0f && x0 > 0.0f) || (m_prevSample >= 0.0f && x0 < 0.0f))
    {
        if (m_zeroCount > 0)
        {
            int subPeriodSamples = m_zeroCount;
            m_measuredFreq = static_cast<float>(m_sampleRate) / static_cast<float>(subPeriodSamples);
        }
        m_zeroCount = 0;
    }
    m_zeroCount++;
    m_prevSample = x0;

    float subFreq = m_measuredFreq / static_cast<float>(octDiv);
    if (subFreq > 0.0f && subFreq < static_cast<float>(m_sampleRate) * 0.45f)
    {
        m_phase += static_cast<float>(subFreq / m_sampleRate);
        if (m_phase >= 1.0f) m_phase -= 1.0f;
    }

    float subOut = (m_phase < 0.5f) ? 0.5f : -0.5f;

    if (m_silenceCounter >= kGateSamples)
        subOut = 0.0f;

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = subOut;
}
