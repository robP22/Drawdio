#include <JuceHeader.h>
#include "Effects/MiscEffects.h"

#include <algorithm>
#include <cmath>

void VcaCompressorEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_envelopeFollower = 0.0f;
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
    float thresh_dB = -50.0f + effectParam * 45.0f;
    float compRatio = 4.0f;

    float inputLevel = 0.0f;
    for (int ch = 0; ch < c; ++ch)
        inputLevel = std::max(inputLevel, std::abs(b[ch][s]));

    if (inputLevel > m_envelopeFollower)
        m_envelopeFollower = m_attackCoeff * m_envelopeFollower + (1.0f - m_attackCoeff) * inputLevel;
    else
        m_envelopeFollower = m_releaseCoeff * m_envelopeFollower + (1.0f - m_releaseCoeff) * inputLevel;

    float env_dB = 20.0f * std::log10(std::fmax(m_envelopeFollower, 1e-8f));

    float gain_dB = 0.0f;
    if (env_dB > thresh_dB)
    {
        float over = env_dB - thresh_dB;
        gain_dB = -over * (1.0f - 1.0f / compRatio);
    }

    float makeup_dB = thresh_dB * -0.3f;
    if (makeup_dB < 0.0f) makeup_dB = 0.0f;
    float gain = std::pow(10.0f, (gain_dB + makeup_dB) / 20.0f);
    gain = std::fmin(1.0f, gain);

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = b[ch][s] * gain;
}

void VcaCompressorEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float vol = params[1];
    float attackMs = 0.5f + vol * 49.5f;
    float attackSec = attackMs * 0.001f;
    double sr = m_sampleRate == 0.0 ? 44100.0 : m_sampleRate;
    m_attackCoeff = static_cast<float>(std::exp(-1.0 / (sr * std::fmax(attackSec, 0.0001))));
    m_releaseCoeff = static_cast<float>(std::exp(-1.0 / (sr * 0.1)));

    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[3]);
}

void SidechainDuckerEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_duckAmount = 0.5f;
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.timer = 0;
        ch.intervalSamples = static_cast<int>(sampleRate * 0.5f);
    }
}

void SidechainDuckerEffect::reset()
{
    for (auto& ch : m_channels)
    {
        ch.timer = 0;
        ch.intervalSamples = static_cast<int>(0.5f * static_cast<float>(m_sampleRate));
    }
}

void SidechainDuckerEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float rate = effectParam;
    float intervalSec = 0.05f + rate * 1.95f;
    int intervalSamps = static_cast<int>(m_sampleRate * intervalSec);
    if (intervalSamps < 1) intervalSamps = 1;
    float duck = m_duckAmount;
    if (duck < 0.0f) duck = 0.0f;
    if (duck > 1.0f) duck = 1.0f;

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& chState = m_channels[static_cast<size_t>(ch)];
        chState.intervalSamples = intervalSamps;

        if (chState.timer >= chState.intervalSamples)
            chState.timer = 0;

        float phase = static_cast<float>(chState.timer) / static_cast<float>(chState.intervalSamples);
        float gain = 1.0f - duck * (1.0f - phase) * (1.0f - phase) * (1.0f - phase);
        if (gain < 0.0f) gain = 0.0f;

        b[ch][s] = b[ch][s] * gain;

        ++chState.timer;
    }
}

void SidechainDuckerEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    m_duckAmount = params[1];
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[3]);
}
