#include "Effects/DelayEffects.h"

#include <algorithm>
#include <cmath>

void ModulatedDelayEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    prepareSimpleDelay(m_delay, sampleRate, 2.0);
    m_lfoPhase = 0.0f;
}

void ModulatedDelayEffect::reset()
{
    resetSimpleDelay(m_delay);
    m_lfoPhase = 0.0f;
}

void ModulatedDelayEffect::processSample(float** b, int c, int s, float effectParam)
{
    float rate = effectParam;
    float modRate = 0.1f + rate * 9.9f;
    size_t bufSize = m_delay.buf.size();
    if (bufSize == 0) return;

    m_lfoPhase += static_cast<float>(modRate / m_sampleRate);
    m_lfoPhase = std::fmod(m_lfoPhase, 1.0f);
    float lfo = std::sin(m_lfoPhase * 2.0f * 3.14159265f);

    int maxDelay = static_cast<int>(m_sampleRate * 0.5);
    int minDelay = static_cast<int>(m_sampleRate * 0.02);
    int delayRange = static_cast<int>((maxDelay - minDelay) * 0.5f);
    int delaySamples = minDelay + static_cast<int>((lfo * 0.5f + 0.5f) * delayRange);

    if (c > 0)
        m_delay.buf[m_delay.writePtr] = b[0][s];

    int readPtr = (static_cast<int>(m_delay.writePtr) - delaySamples) % static_cast<int>(bufSize);
    if (readPtr < 0) readPtr += static_cast<int>(bufSize);

    float wet = m_delay.buf[static_cast<size_t>(readPtr)];

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = wet;

    m_delay.writePtr = (m_delay.writePtr + 1) % bufSize;
}

void SimpleDelayEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    prepareSimpleDelay(m_delay, sampleRate, 1.0);
}

void SimpleDelayEffect::reset()
{
    resetSimpleDelay(m_delay);
}

void SimpleDelayEffect::processSample(float** b, int c, int s, float effectParam)
{
    float delaySec = 0.1f + effectParam * 0.9f;
    int delaySamples = static_cast<int>(m_sampleRate * delaySec);
    float feedback = 0.4f;

    size_t bufSize = m_delay.buf.size();
    if (bufSize == 0) return;

    if (c > 0)
    {
        float in = b[0][s];
        m_delay.buf[m_delay.writePtr] = in;
        size_t readPtr = (m_delay.writePtr + bufSize - static_cast<size_t>(delaySamples)) % bufSize;
        float delayed = m_delay.buf[readPtr];
        m_delay.buf[m_delay.writePtr] = in + delayed * feedback;
        b[0][s] = delayed;
        if (c > 1)
            b[1][s] = delayed;
    }
    m_delay.writePtr = (m_delay.writePtr + 1) % bufSize;
}

void DynamicRingBufferEffect::prepare(double sampleRate, int)
{
    prepareRingBuffer(m_buffer, sampleRate, 4.0);
}

void DynamicRingBufferEffect::reset()
{
    resetRingBuffer(m_buffer);
}

void DynamicRingBufferEffect::processSample(float** b, int c, int s, float effectParam)
{
    float size = effectParam;
    size_t bufSize = m_buffer.buf.size();
    if (bufSize == 0) return;

    size_t loopLen = static_cast<size_t>((0.1 + size * 0.9) * bufSize);
    float readSpeed = 0.75f;

    if (c > 0)
        m_buffer.buf[m_buffer.writePtr] = b[0][s];

    size_t baseRead = (m_buffer.writePtr + bufSize - loopLen) % bufSize;
    size_t readIdx = baseRead + static_cast<size_t>(m_buffer.readHead) % loopLen;
    if (readIdx >= bufSize) readIdx -= bufSize;

    float sample = m_buffer.buf[readIdx];

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = sample;

    m_buffer.readHead += readSpeed;
    if (m_buffer.readHead >= static_cast<float>(loopLen))
        m_buffer.readHead -= static_cast<float>(loopLen);

    m_buffer.writePtr = (m_buffer.writePtr + 1) % bufSize;
}

void TapeStopEchoEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_buf.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
    m_writePtr = 0;
    m_readHead = 0.0f;
}

void TapeStopEchoEffect::reset()
{
    std::fill(m_buf.begin(), m_buf.end(), 0.0f);
    m_writePtr = 0;
    m_readHead = 0.0f;
}

void TapeStopEchoEffect::processSample(float** b, int c, int s, float effectParam)
{
    float braking = effectParam;
    size_t bufSize = m_buf.size();
    if (bufSize == 0) return;

    float brakeFactor = 0.999f - braking * 0.019f;

    if (c > 0)
        m_buf[m_writePtr] = b[0][s];

    float readSpeed = 1.0f;
    if (braking > 0.01f)
        readSpeed = m_readHead * brakeFactor + 0.001f;

    size_t readIdx;
    if (readSpeed >= 0)
    {
        size_t base = (m_writePtr + bufSize - 1) % bufSize;
        size_t offset = static_cast<size_t>(readSpeed * 100.0f) % bufSize;
        readIdx = (base + bufSize - offset) % bufSize;
    }
    else
    {
        size_t offset = static_cast<size_t>(-readSpeed * 100.0f) % bufSize;
        readIdx = (m_writePtr + offset) % bufSize;
    }

    float sample = m_buf[readIdx];

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = sample;

    m_readHead = readSpeed;
    m_writePtr = (m_writePtr + 1) % bufSize;
}
