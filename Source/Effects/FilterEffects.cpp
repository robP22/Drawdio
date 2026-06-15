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
    juce::ScopedNoDenormals noDenorm;
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

void SpectralFreezeEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
        ch.writePtr = 0;
        ch.readPos = 0.0f;
        ch.lfoPhase = 0.0f;
        ch.freezeLen = static_cast<size_t>(sampleRate * 1.0);
        ch.wasFrozen = false;
    }
}

void SpectralFreezeEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.readPos = 0.0f;
        ch.lfoPhase = 0.0f;
        ch.wasFrozen = false;
    }
}

void SpectralFreezeEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    bool frozen = (effectParam >= 0.05f);
    float pitchRatio = 0.25f + effectParam * 1.75f;

    static constexpr float kLfoRate = 0.2f;
    static constexpr float kLfoDepth = 0.05f;

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& fc = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = fc.buf.size();
        if (bufSize == 0) continue;

        fc.buf[fc.writePtr] = b[ch][s];

        if (!frozen)
        {
            fc.readPos = static_cast<float>(fc.writePtr);
            b[ch][s] = fc.buf[fc.writePtr];
            fc.wasFrozen = false;
        }
        else
        {
            if (!fc.wasFrozen)
            {
                fc.readPos = static_cast<float>((fc.writePtr + bufSize - fc.freezeLen) % bufSize);
                fc.wasFrozen = true;
                fc.lfoPhase = 0.0f;
            }

            fc.lfoPhase += static_cast<float>(kLfoRate / m_sampleRate);
            if (fc.lfoPhase >= 1.0f) fc.lfoPhase -= 1.0f;
            float lfo = std::sin(fc.lfoPhase * 2.0f * 3.14159265f);

            fc.readPos += pitchRatio + lfo * kLfoDepth;
            if (fc.readPos >= static_cast<float>(fc.freezeLen))
                fc.readPos -= static_cast<float>(fc.freezeLen);

            size_t base = (fc.writePtr + bufSize - fc.freezeLen) % bufSize;
            float absPos = base + fc.readPos;
            if (absPos >= static_cast<float>(bufSize))
                absPos -= static_cast<float>(bufSize);

            size_t idx = static_cast<size_t>(absPos) % bufSize;
            float frac = absPos - std::floor(absPos);
            size_t next = (idx + 1) % bufSize;
            b[ch][s] = fc.buf[idx] * (1.0f - frac) + fc.buf[next] * frac;
        }

        fc.writePtr = (fc.writePtr + 1) % bufSize;
    }
}

void FormantShifterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_lp1.assign(static_cast<size_t>(numChannels), 0.0f);
    m_lp2.assign(static_cast<size_t>(numChannels), 0.0f);
    m_envState = 0.0f;
}

void FormantShifterEffect::reset()
{
    std::fill(m_lp1.begin(), m_lp1.end(), 0.0f);
    std::fill(m_lp2.begin(), m_lp2.end(), 0.0f);
    m_envState = 0.0f;
}

void FormantShifterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float formantFreq = effectParam;

    float attackCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.002)));
    float releaseCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.1)));

    float inputLevel = 0.0f;
    for (int ch = 0; ch < c; ++ch)
        inputLevel = std::max(inputLevel, std::abs(b[ch][s]));

    if (inputLevel > m_envState)
        m_envState = attackCoeff * m_envState + (1.0f - attackCoeff) * inputLevel;
    else
        m_envState = releaseCoeff * m_envState + (1.0f - releaseCoeff) * inputLevel;

    float envMod = (m_envState > 0.5f) ? 1.0f : (m_envState / 0.5f);
    float centerHz = 200.0f + formantFreq * 1800.0f * (0.3f + envMod * 0.7f);

    float fc = centerHz / static_cast<float>(m_sampleRate);
    float bwHz = std::max(50.0f, centerHz / 5.0f);
    float R = std::exp(-3.14159265f * bwHz / static_cast<float>(m_sampleRate));
    float theta = 2.0f * 3.14159265f * fc;
    float cosTheta = std::cos(theta);

    float b0 = 0.5f * (1.0f - R * R);
    float a1 = -2.0f * R * cosTheta;
    float a2 = R * R;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        float& s1 = m_lp1[static_cast<size_t>(ch)];
        float& s2 = m_lp2[static_cast<size_t>(ch)];
        float y = b0 * x + s1;
        s1 = -a1 * y + s2;
        s2 = b0 * (-x) - a2 * y;
        b[ch][s] = y;
    }
}
