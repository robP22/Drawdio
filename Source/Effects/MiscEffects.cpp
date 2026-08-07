#include <JuceHeader.h>
#include "Effects/MiscEffects.h"

#include <algorithm>
#include <cmath>

void VcaCompressorEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_envelopeFollower = 0.0f;
    m_makeupGain = 1.0f;
    float defaultAttackMs = 2.0f;
    float attackSec = defaultAttackMs * 0.001f;
    double sr = m_sampleRate == 0.0 ? 44100.0 : m_sampleRate;
    m_attackCoeff = static_cast<float>(std::exp(-1.0 / (sr * std::fmax(attackSec, 0.0001))));
    m_releaseCoeff = static_cast<float>(std::exp(-1.0 / (sr * 0.1)));
}

void VcaCompressorEffect::reset()
{
    m_envelopeFollower = 0.0f;
}

void VcaCompressorEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float thresh_dB = -45.0f + effectParam * 40.0f;
    float compRatio = 4.0f;
    float kneeWidth = 6.0f;

    float inputLevel = 0.0f;
    for (int ch = 0; ch < c; ++ch)
        inputLevel = std::max(inputLevel, std::abs(b[ch][s]));

    if (inputLevel > m_envelopeFollower)
        m_envelopeFollower = m_attackCoeff * m_envelopeFollower + (1.0f - m_attackCoeff) * inputLevel;
    else
        m_envelopeFollower = m_releaseCoeff * m_envelopeFollower + (1.0f - m_releaseCoeff) * inputLevel;

    float env_dB = 20.0f * std::log10(std::fmax(m_envelopeFollower, 1e-8f));

    float gain_dB = 0.0f;
    float over = env_dB - thresh_dB;
    if (over > kneeWidth)
        gain_dB = -over * (1.0f - 1.0f / compRatio);
    else if (over > -kneeWidth)
    {
        float kneeT = (over + kneeWidth) / (2.0f * kneeWidth);
        gain_dB = -over * (1.0f - 1.0f / compRatio) * kneeT * kneeT;
    }

    float makeup_dB = 20.0f * std::log10(std::fmax(m_makeupGain, 0.01f));
    float gain = std::pow(10.0f, (gain_dB + makeup_dB) / 20.0f);
    if (gain < 0.0f) gain = 0.0f;

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = b[ch][s] * gain;
}

void VcaCompressorEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float attackMs = 0.5f + params[0] * 49.5f;
    float releaseMs = 10.0f + params[1] * 490.0f;
    m_makeupGain = 0.5f + params[3] * 1.5f;

    double sr = m_sampleRate == 0.0 ? 44100.0 : m_sampleRate;
    m_attackCoeff = static_cast<float>(std::exp(-1.0 / (sr * std::fmax(attackMs * 0.001f, 0.0001))));
    m_releaseCoeff = static_cast<float>(std::exp(-1.0 / (sr * std::fmax(releaseMs * 0.001f, 0.001))));

    float thresh_dB = -45.0f + params[2] * 40.0f;
    float compRatio = 4.0f;
    float kneeWidth = 6.0f;
    float makeup_dB = 20.0f * std::log10(std::fmax(m_makeupGain, 0.01f));
    constexpr float dB20ToLinearExp2 = 0.16609640474f;   // log2(10.0f) / 20.0f  =>  exp2f(x * k) == 10^(x/20)

    for (int s = 0; s < n; ++s)
    {
        float inputLevel = 0.0f;
        for (int ch = 0; ch < c; ++ch)
            inputLevel = std::max(inputLevel, std::abs(b[ch][s]));

        if (inputLevel > m_envelopeFollower)
            m_envelopeFollower = m_attackCoeff * m_envelopeFollower + (1.0f - m_attackCoeff) * inputLevel;
        else
            m_envelopeFollower = m_releaseCoeff * m_envelopeFollower + (1.0f - m_releaseCoeff) * inputLevel;

        float env_dB = 20.0f * std::log10(std::fmax(m_envelopeFollower, 1e-8f));

        float gain_dB = 0.0f;
        float over = env_dB - thresh_dB;
        if (over > kneeWidth)
            gain_dB = -over * (1.0f - 1.0f / compRatio);
        else if (over > -kneeWidth)
        {
            float kneeT = (over + kneeWidth) / (2.0f * kneeWidth);
            gain_dB = -over * (1.0f - 1.0f / compRatio) * kneeT * kneeT;
        }

        float gain = std::exp2f((gain_dB + makeup_dB) * dB20ToLinearExp2);
        if (gain < 0.0f) gain = 0.0f;

        for (int ch = 0; ch < c; ++ch)
            b[ch][s] *= gain;
    }
}

void RhythmGateEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_phase = 0;
    m_smoothEnv = 1.0f;
    m_smoothCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.001f));
}

void RhythmGateEffect::reset()
{
    m_phase = 0;
    m_smoothEnv = 1.0f;
}

void RhythmGateEffect::processSample(float** b, int c, int s, float)
{
    juce::ScopedNoDenormals noDenorm;
    for (int ch = 0; ch < c; ++ch)
        b[ch][s] *= m_smoothEnv;
}

void RhythmGateEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;

    const float rateParam = params[0];
    const float shapeParam = params[1];
    const float depth = std::max(0.0f, std::min(1.0f, params[2]));

    const float sr = static_cast<float>(m_sampleRate);
    const float cycleSec = 0.05f + rateParam * 9.95f;
    int cycleSamples = static_cast<int>(sr * cycleSec);
    if (cycleSamples < 2) cycleSamples = 2;

    if (m_phase >= cycleSamples)
        m_phase %= cycleSamples;

    const float invCycleSamples = 1.0f / static_cast<float>(cycleSamples);

    for (int s = 0; s < n; ++s)
    {
        const float phase = static_cast<float>(m_phase) * invCycleSamples;

        float env;
        if (shapeParam < 0.33f)
        {
            const float t = shapeParam / 0.33f;
            const float sine = 0.5f + 0.5f * std::sin(phase * 6.2831853f);
            const float tri  = 1.0f - 2.0f * std::fabs(phase - 0.5f);
            env = sine + (tri - sine) * t;
        }
        else if (shapeParam <= 0.66f)
        {
            const float t = (shapeParam - 0.33f) / 0.33f;
            const float exponent = 1.5f + t * 6.0f;
            env = 1.0f - std::pow(1.0f - phase, exponent);
        }
        else
        {
            const float t = (shapeParam - 0.66f) / 0.34f;
            const float duty = 0.5f + t * 0.3f;
            env = (phase < duty) ? 1.0f : 0.0f;
        }

        float gain = 1.0f - depth * (1.0f - env);
        if (gain < 0.0f) gain = 0.0f;
        if (gain > 1.0f) gain = 1.0f;

        m_smoothEnv += (gain - m_smoothEnv) * m_smoothCoeff;

        for (int ch = 0; ch < c; ++ch)
            b[ch][s] *= m_smoothEnv;

        ++m_phase;
        if (m_phase >= cycleSamples)
            m_phase = 0;
    }
}
