#include <JuceHeader.h>
#include "Effects/MiscEffects.h"

#include <algorithm>
#include <cmath>

void VcaCompressorEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_envelopeFollower = 0.0f;
    m_attackMs = 2.0f;
    setVolumeParam(m_attackMs * 0.2f);
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

void VcaCompressorEffect::setVolumeParam(float vol)
{
    m_attackMs = 0.5f + vol * 49.5f;
    float attackSec = m_attackMs * 0.001f;
    double sr = m_sampleRate == 0.0 ? 44100.0 : m_sampleRate;
    m_attackCoeff = static_cast<float>(std::exp(-1.0 / (sr * std::fmax(attackSec, 0.0001))));
    m_releaseCoeff = static_cast<float>(std::exp(-1.0 / (sr * 0.1)));
}

void SampleRateDegraderEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_sampleHold = 0;
    m_heldValues.assign(static_cast<size_t>(numChannels), 0.0f);
}

void SampleRateDegraderEffect::reset()
{
    m_sampleHold = 0;
    std::fill(m_heldValues.begin(), m_heldValues.end(), 0.0f);
}

void SampleRateDegraderEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float bits = effectParam;

    int bitDepth = 1 + static_cast<int>(bits * 15.0f);
    if (bitDepth < 1) bitDepth = 1;
    if (bitDepth > 24) bitDepth = 24;
    int holdLen = 1 + static_cast<int>((1.0f - bits) * 31.0f);
    if (holdLen < 1) holdLen = 1;

    int chCount = std::min(c, static_cast<int>(m_heldValues.size()));
    if (--m_sampleHold <= 0)
    {
        m_sampleHold = holdLen;
        for (int ch = 0; ch < chCount; ++ch)
        {
            float x = b[ch][s];
            float maxVal = static_cast<float>((1 << bitDepth) - 1);
            m_heldValues[static_cast<size_t>(ch)] = std::round(x * maxVal) / maxVal;
        }
    }

    for (int ch = 0; ch < chCount; ++ch)
        b[ch][s] = m_heldValues[static_cast<size_t>(ch)];
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

void SidechainDuckerEffect::setVolumeParam(float vol)
{
    m_duckAmount = vol;
}
