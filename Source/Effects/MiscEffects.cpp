#include "Effects/MiscEffects.h"

#include <algorithm>
#include <cmath>

void VcaCompressorEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_envelopeFollower = 0.0f;
}

void VcaCompressorEffect::reset()
{
    m_envelopeFollower = 0.0f;
}

void VcaCompressorEffect::processSample(float** b, int c, int s, float effectParam)
{
    float threshold = effectParam;
    float thresh = 0.1f + threshold * 0.8f;
    float compRatio = 4.0f;

    float attackCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.002)));
    float releaseCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.1)));

    float inputLevel = 0.0f;
    for (int ch = 0; ch < c; ++ch)
        inputLevel = std::max(inputLevel, std::abs(b[ch][s]));

    if (inputLevel > m_envelopeFollower)
        m_envelopeFollower = attackCoeff * m_envelopeFollower + (1.0f - attackCoeff) * inputLevel;
    else
        m_envelopeFollower = releaseCoeff * m_envelopeFollower + (1.0f - releaseCoeff) * inputLevel;

    float gain = 1.0f;
    if (m_envelopeFollower > thresh)
    {
        float over = m_envelopeFollower - thresh;
        float reduction = over * (1.0f - 1.0f / compRatio);
        gain = (thresh + over - reduction) / (thresh + over);
    }

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = b[ch][s] * gain;
}

void SampleRateDegraderEffect::prepare(double, int)
{
    m_sampleHold = 0;
    m_heldValue = 0.0f;
}

void SampleRateDegraderEffect::reset()
{
    m_sampleHold = 0;
    m_heldValue = 0.0f;
}

void SampleRateDegraderEffect::processSample(float** b, int c, int s, float effectParam)
{
    float bits = effectParam;

    int bitDepth = 1 + static_cast<int>(bits * 15.0f);
    int holdLen = 1 + static_cast<int>((1.0f - bits) * 31.0f);

    if (m_sampleHold <= 0)
    {
        m_sampleHold = holdLen;
        for (int ch = 0; ch < c; ++ch)
        {
            float x = b[ch][s];
            float maxVal = static_cast<float>((1 << bitDepth) - 1);
            m_heldValue = std::round(x * maxVal) / maxVal;
        }
    }
    else
    {
        --m_sampleHold;
    }

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = m_heldValue;
}
