#include <JuceHeader.h>
#include "Effects/DelayEffects.h"

#include <algorithm>
#include <cmath>

void MicroPitchChorusEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 0.5), 0.0f);
        ch.writePtr = 0;
        ch.readPos1 = 0.0f;
        ch.readPos2 = 0.0f;
        ch.lfoPhase = 0.0f;
    }
}

void MicroPitchChorusEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.readPos1 = 0.0f;
        ch.readPos2 = 0.0f;
        ch.lfoPhase = 0.0f;
    }
}

void MicroPitchChorusEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float detuneCents = effectParam * 50.0f;
    float pitch1 = 1.0f + detuneCents / 1200.0f;
    float pitch2 = 1.0f - detuneCents / 1200.0f;

    static constexpr float kLfoRate = 0.3f;
    static constexpr float kLfoDepthSamples = 0.002f;  // 2ms

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = mc.buf.size();
        if (bufSize == 0) continue;

        mc.buf[mc.writePtr] = b[ch][s];

        mc.lfoPhase += static_cast<float>(kLfoRate / m_sampleRate);
        if (mc.lfoPhase >= 1.0f) mc.lfoPhase -= 1.0f;
        float lfo1 = std::sin(mc.lfoPhase * 2.0f * 3.14159265f);
        float lfo2 = std::sin((mc.lfoPhase + 0.5f) * 2.0f * 3.14159265f);

        float mod1 = lfo1 * kLfoDepthSamples * static_cast<float>(m_sampleRate);
        float mod2 = lfo2 * kLfoDepthSamples * static_cast<float>(m_sampleRate);

        mc.readPos1 += pitch1 + mod1 * 0.001f;
        if (mc.readPos1 >= static_cast<float>(bufSize))
            mc.readPos1 -= static_cast<float>(bufSize);
        else if (mc.readPos1 < 0.0f)
            mc.readPos1 += static_cast<float>(bufSize);

        mc.readPos2 += pitch2 + mod2 * 0.001f;
        if (mc.readPos2 >= static_cast<float>(bufSize))
            mc.readPos2 -= static_cast<float>(bufSize);
        else if (mc.readPos2 < 0.0f)
            mc.readPos2 += static_cast<float>(bufSize);

        auto readTap = [&](float pos) -> float {
            size_t idx = static_cast<size_t>(pos) % bufSize;
            float frac = pos - std::floor(pos);
            size_t next = (idx + 1) % bufSize;
            return mc.buf[idx] * (1.0f - frac) + mc.buf[next] * frac;
        };

        float tap1 = readTap(mc.readPos1);
        float tap2 = readTap(mc.readPos2);
        b[ch][s] = tap1 * 0.35f + tap2 * 0.35f;

        mc.writePtr = (mc.writePtr + 1) % bufSize;
    }
}

void SimpleDelayEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_delays.resize(static_cast<size_t>(numChannels));
    for (auto& d : m_delays)
        prepareSimpleDelay(d, sampleRate, 1.0);
}

void SimpleDelayEffect::reset()
{
    for (auto& d : m_delays)
        resetSimpleDelay(d);
}

void SimpleDelayEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float delaySec = 0.1f + effectParam * 0.9f;
    float feedback = 0.1f + effectParam * 0.6f;

    int chCount = std::min(c, static_cast<int>(m_delays.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& d = m_delays[static_cast<size_t>(ch)];
        size_t bufSize = d.buf.size();
        if (bufSize == 0) continue;

        size_t delaySamples = static_cast<size_t>(m_sampleRate * delaySec);
        if (delaySamples >= bufSize) delaySamples = bufSize - 1;

        float in = b[ch][s];
        d.buf[d.writePtr] = in;
        size_t readPtr = (d.writePtr + bufSize - delaySamples) % bufSize;
        float delayed = d.buf[readPtr];
        d.buf[d.writePtr] = in + delayed * feedback;
        b[ch][s] = delayed;
        d.writePtr = (d.writePtr + 1) % bufSize;
    }
}

void DynamicRingBufferEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_buffers.resize(static_cast<size_t>(numChannels));
    for (auto& b : m_buffers)
        prepareRingBuffer(b, sampleRate, 4.0);
}

void DynamicRingBufferEffect::reset()
{
    for (auto& b : m_buffers)
        resetRingBuffer(b);
}

void DynamicRingBufferEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float size = effectParam;
    float readSpeed = 0.25f + effectParam * 0.75f;

    int chCount = std::min(c, static_cast<int>(m_buffers.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& buf = m_buffers[static_cast<size_t>(ch)];
        size_t bufSize = buf.buf.size();
        if (bufSize == 0) continue;

        size_t loopLen = static_cast<size_t>((0.15f + size * 0.85f) * bufSize);
        if (loopLen == 0) loopLen = 1;

        size_t baseRead = (buf.writePtr + bufSize - loopLen) % bufSize;
        float safeHead = std::fmax(0.0f, buf.readHead);
        size_t readIdx = (baseRead + static_cast<size_t>(safeHead) % loopLen) % bufSize;

        float delayed = buf.buf[readIdx];
        buf.buf[buf.writePtr] = b[ch][s] + 0.4f * delayed;

        b[ch][s] = delayed;

        buf.readHead = std::fmod(buf.readHead + readSpeed, static_cast<float>(loopLen));

        buf.writePtr = (buf.writePtr + 1) % bufSize;
    }
}

void TapeStopEchoEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
        ch.writePtr = 0;
        ch.readSpeed = 1.0f;
        float delaySamps = std::fmin(0.1f * static_cast<float>(sampleRate), static_cast<float>(ch.buf.size() - 1));
        ch.readPos = static_cast<float>((ch.writePtr + ch.buf.size() - static_cast<size_t>(delaySamps)) % ch.buf.size());
    }
}

void TapeStopEchoEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.readPos = 0.0f;
        ch.readSpeed = 1.0f;
    }
}

void TapeStopEchoEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float braking = effectParam;
    float brakeFactor = 0.98f - braking * 0.05f;

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& chState = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = chState.buf.size();
        if (bufSize == 0) continue;

        chState.buf[chState.writePtr] = b[ch][s];

        if (braking > 0.01f)
            chState.readSpeed = std::fmax(0.001f, chState.readSpeed * brakeFactor + 0.001f);
        else
            chState.readSpeed = 1.0f;

        chState.readPos += chState.readSpeed;
        if (chState.readPos >= static_cast<float>(bufSize))
            chState.readPos -= static_cast<float>(bufSize);

        size_t idx = static_cast<size_t>(chState.readPos);
        float frac = chState.readPos - static_cast<float>(idx);
        size_t next = (idx + 1) % bufSize;
        b[ch][s] = chState.buf[idx] * (1.0f - frac) + chState.buf[next] * frac;

        chState.writePtr = (chState.writePtr + 1) % bufSize;
    }
}
