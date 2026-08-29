#include <JuceHeader.h>
#include "Effects/FilterEffects.h"

#include <algorithm>
#include <cmath>

void SpectralFreezeEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[1]);
}

void SpectralFreezeEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 1.5), 0.0f);
        ch.freezeLen = static_cast<size_t>(sampleRate * 1.0);
        ch.freezeBuf.assign(ch.freezeLen, 0.0f);
        ch.writePtr = 0;
        ch.readPos = 0.0f;
        ch.wasFrozen = false;
    }
}

void SpectralFreezeEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        std::fill(ch.freezeBuf.begin(), ch.freezeBuf.end(), 0.0f);
        ch.writePtr = 0;
        ch.readPos = 0.0f;
        ch.wasFrozen = false;
        ch.entryXfadePos = FreezeChannel::kXfadeLen;
        ch.exitXfadePos = FreezeChannel::kXfadeLen;
        ch.exitXfadeFrom = 0.0f;
    }
}

void SpectralFreezeEffect::processSample(float** b, int c, int s, float effectParam)
{
    bool frozen = (effectParam >= 0.05f);
    float pitchRatio = 0.25f + effectParam * 1.75f;

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& fc = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = fc.buf.size();
        if (bufSize == 0) continue;

        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        fc.buf[fc.writePtr] = in;

        if (!frozen)
        {
            float live = fc.buf[fc.writePtr];
            if (fc.wasFrozen)
            {
                fc.wasFrozen = false;
                fc.exitXfadePos = 0.0f;
            }
            if (fc.exitXfadePos < FreezeChannel::kXfadeLen)
            {
                float w = fc.exitXfadePos / FreezeChannel::kXfadeLen;
                b[ch][s] = fc.exitXfadeFrom * (1.0f - w) + live * w;
                fc.exitXfadePos += 1.0f;
            }
            else
            {
                b[ch][s] = live;
            }
        }
        else
        {
            if (!fc.wasFrozen)
            {
                fc.wasFrozen = true;
                fc.entryXfadePos = 0.0f;
                for (size_t i = 0; i < fc.freezeLen; ++i)
                    fc.freezeBuf[i] = fc.buf[(fc.writePtr + bufSize - fc.freezeLen + i) % bufSize];
            }

            float step = pitchRatio;
            fc.readPos += step;
            if (fc.readPos >= static_cast<float>(fc.freezeLen))
                fc.readPos -= static_cast<float>(fc.freezeLen);

            float out = interpolateDelayRead(fc.freezeBuf, fc.readPos);
            float wrapStart = fc.readPos - (static_cast<float>(fc.freezeLen) - FreezeChannel::kXfadeLen);
            if (wrapStart >= 0.0f)
            {
                float a = wrapStart / FreezeChannel::kXfadeLen;
                float w = 0.5f * (1.0f - std::cos(3.14159265f * a));
                out = out * (1.0f - w)
                    + interpolateDelayRead(fc.freezeBuf, wrapStart) * w;
            }
            b[ch][s] = out;

            if (fc.entryXfadePos < FreezeChannel::kXfadeLen)
            {
                float w = fc.entryXfadePos / FreezeChannel::kXfadeLen;
                b[ch][s] = in * (1.0f - w) + b[ch][s] * w;
                fc.entryXfadePos += 1.0f;
            }

            fc.exitXfadeFrom = b[ch][s];
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
    m_attackCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.002)));
    m_releaseCoeff = static_cast<float>(std::exp(-1.0 / (m_sampleRate * 0.1)));
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

    float inputLevel = 0.0f;
    for (int ch = 0; ch < c; ++ch)
        inputLevel = std::max(inputLevel, std::abs(b[ch][s]));

    if (inputLevel > m_envState)
        m_envState = m_attackCoeff * m_envState + (1.0f - m_attackCoeff) * inputLevel;
    else
        m_envState = m_releaseCoeff * m_envState + (1.0f - m_releaseCoeff) * inputLevel;

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

    int chCount = std::min(c, static_cast<int>(m_lp1.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float x = b[ch][s];
        float& s1 = m_lp1[static_cast<size_t>(ch)];
        float& s2 = m_lp2[static_cast<size_t>(ch)];
        float y = b0 * x + s1;
        s1 = -a1 * y + s2;
        s2 = b0 * (-x) - a2 * y;
        if (!std::isfinite(y))
        {
            y = 0.0f;
            s1 = 0.0f;
            s2 = 0.0f;
        }
        b[ch][s] = y;
    }
}

void MultiModeFilterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float cutoffParam = params[2];
    float cutoffEnd = 20.0f + (20000.0f - 20.0f) * cutoffParam * cutoffParam;
    float maxCut = static_cast<float>(m_sampleRate) * 0.45f;
    if (cutoffEnd > maxCut) cutoffEnd = maxCut;
    float cutoffStart = m_prevCutoffHz;
    m_prevCutoffHz = cutoffEnd;

    float bandPos = params[0] * 3.0f;
    int mode = std::min(2, static_cast<int>(bandPos));
    float withinBand = bandPos - static_cast<float>(mode);
    float R = 1.0f - withinBand * 0.9f;
    if (R > 0.98f) R = 0.98f;
    const float srF = static_cast<float>(m_sampleRate);
    const float twoRg = 2.0f * R;

    int maxCh = std::min(c, static_cast<int>(m_states.size()));
    for (int ch = 0; ch < maxCh; ++ch)
    {
        auto& st = m_states[static_cast<size_t>(ch)];
        for (int s = 0; s < n; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(n);
            const float fcHz = cutoffStart + (cutoffEnd - cutoffStart) * t;
            const float g = std::tan(3.14159265f * fcHz / srF);
            const float invScale = 1.0f / (1.0f + twoRg * g + g * g);

            float x = b[ch][s];
            float hp = (x - (twoRg + g) * st.bp - st.lp) * invScale;
            st.bp = g * hp + st.bp;
            st.lp = g * st.bp + st.lp;
            if (!std::isfinite(hp) || !std::isfinite(st.bp) || !std::isfinite(st.lp))
            {
                st = {};
                hp = 0.0f;
                st.bp = 0.0f;
                st.lp = 0.0f;
            }
            if (std::abs(st.lp) > 8.0f) st.lp = (st.lp >= 0.0f) ? 8.0f : -8.0f;
            if (std::abs(st.bp) > 8.0f) st.bp = (st.bp >= 0.0f) ? 8.0f : -8.0f;

            float out;
            switch (mode)
            {
                case 0: out = st.lp; break;
                case 1: out = st.bp; break;
                default: out = hp; break;
            }
            b[ch][s] = out;
        }
    }
}

void MultiModeFilterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_states.resize(static_cast<size_t>(numChannels));
}

void MultiModeFilterEffect::reset()
{
    for (auto& s : m_states)
        s = {};
    m_prevCutoffHz = 20.0f;
}

void MultiModeFilterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float fcHz = 20.0f + (20000.0f - 20.0f) * effectParam * effectParam;
    float maxCut = static_cast<float>(m_sampleRate) * 0.45f;
    if (fcHz > maxCut) fcHz = maxCut;
    float g = std::tan(3.14159265f * fcHz / static_cast<float>(m_sampleRate));
    float R = 1.0f;
    float invScale = 1.0f / (1.0f + 2.0f * R * g + g * g);

    int maxCh = std::min(c, static_cast<int>(m_states.size()));
    for (int ch = 0; ch < maxCh; ++ch)
    {
        auto& st = m_states[static_cast<size_t>(ch)];
        float x = b[ch][s];
        float hp = (x - (2.0f * R + g) * st.bp - st.lp) * invScale;
        st.bp = g * hp + st.bp;
        st.lp = g * st.bp + st.lp;
        if (!std::isfinite(hp) || !std::isfinite(st.bp) || !std::isfinite(st.lp))
        {
            st = {};
            hp = 0.0f;
            st.bp = 0.0f;
            st.lp = 0.0f;
        }
        if (std::abs(st.lp) > 8.0f) st.lp = (st.lp >= 0.0f) ? 8.0f : -8.0f;
        if (std::abs(st.bp) > 8.0f) st.bp = (st.bp >= 0.0f) ? 8.0f : -8.0f;
        b[ch][s] = st.lp;
    }
}

void FormantShifterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float formantFreq = params[2];
    float shiftHz = juce::jlimit(0.0f, 1.0f, params[1]) * 2400.0f;
    float qFactor = 0.5f + juce::jlimit(0.0f, 1.0f, params[3]) * 9.5f;

    float b0 = 0.5f, a1 = 0.0f, a2 = 0.0f;

    for (int s = 0; s < n; ++s)
    {
        float inputLevel = 0.0f;
        for (int ch = 0; ch < c; ++ch)
            inputLevel = std::max(inputLevel, std::abs(b[ch][s]));

        if (inputLevel > m_envState)
            m_envState = m_attackCoeff * m_envState + (1.0f - m_attackCoeff) * inputLevel;
        else
            m_envState = m_releaseCoeff * m_envState + (1.0f - m_releaseCoeff) * inputLevel;

        {
            float envMod = std::min(1.0f, m_envState / 0.5f);
            float modFreq = formantFreq * (0.3f + envMod * 0.7f);
            float centerHz = 200.0f + modFreq * 1800.0f + shiftHz;
            float fc = centerHz / static_cast<float>(m_sampleRate);
            float bwHz = std::max(50.0f, centerHz / 5.0f) / qFactor;
            float R = std::exp(-3.14159265f * bwHz / static_cast<float>(m_sampleRate));
            if (R > 0.995f) R = 0.995f;
            float cosTheta = std::cos(2.0f * 3.14159265f * fc);
            b0 = 0.5f * (1.0f - R * R);
            a1 = -2.0f * R * cosTheta;
            a2 = R * R;
        }

        int chCount = std::min(c, static_cast<int>(m_lp1.size()));
        for (int ch = 0; ch < chCount; ++ch)
        {
            float x = b[ch][s];
            float& s1 = m_lp1[static_cast<size_t>(ch)];
            float& s2 = m_lp2[static_cast<size_t>(ch)];
            float y = b0 * x + s1;
            s1 = -a1 * y + s2;
            s2 = b0 * (-x) - a2 * y;
            if (!std::isfinite(y))
            {
                y = 0.0f;
                s1 = 0.0f;
                s2 = 0.0f;
            }
            b[ch][s] = y;
        }
    }
}
