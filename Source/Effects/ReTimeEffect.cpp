#include <JuceHeader.h>
#include "Effects/ReTimeEffect.h"
#include <algorithm>
#include <cmath>

void ReTimeEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    const size_t bufferSize = static_cast<size_t>(std::max(1.0, sampleRate * 16.0));
    m_channels.resize(static_cast<size_t>(std::max(1, numChannels)));
    for (auto& channel : m_channels)
    {
        channel.ring.assign(bufferSize, 0.0f);
        channel.freeze.assign(bufferSize, 0.0f);
        channel.writePos = 0;
        channel.smoothState = 0.0f;
    }

    m_loopLength = static_cast<float>(sampleRate * 2.0);
    m_phase = 0.0f;
    m_speed = 0.5f;
    m_smoothAlpha = 1.0f - std::exp(-1.0f / (0.015f * static_cast<float>(sampleRate)));
    m_needsSync = true;
    m_hasCaptured = false;
    m_hasTail = false;
}

void ReTimeEffect::reset()
{
    for (auto& channel : m_channels)
    {
        std::fill(channel.ring.begin(), channel.ring.end(), 0.0f);
        std::fill(channel.freeze.begin(), channel.freeze.end(), 0.0f);
        channel.writePos = 0;
        channel.smoothState = 0.0f;
    }

    m_phase = 0.0f;
    m_needsSync = true;
    m_hasCaptured = false;
    m_hasTail = false;
}

void ReTimeEffect::setTransport(float bpm, double ppqPosition, bool isPlaying)
{
    const float safeBpm = std::isfinite(bpm) && bpm > 1.0f ? bpm : 120.0f;

    if (isPlaying && !m_isPlaying)
        m_needsSync = true;

    m_bpm = safeBpm;
    m_ppqPosition = std::isfinite(ppqPosition) ? ppqPosition : 0.0;
    m_isPlaying = isPlaying;
    m_hasTransport = true;
}

float ReTimeEffect::wrapPosition(float position, float size)
{
    if (size <= 0.0f)
        return 0.0f;
    position = std::fmod(position, size);
    if (position < 0.0f)
        position += size;
    return position;
}

float ReTimeEffect::readInterpolated(const ChannelState& channel, float position) const
{
    const float size = static_cast<float>(channel.freeze.size());
    if (size <= 1.0f)
        return 0.0f;

    const float wrapped = wrapPosition(position, size);
    const auto i0 = static_cast<size_t>(wrapped);
    const auto i1 = (i0 + 1) % channel.freeze.size();
    const float frac = wrapped - static_cast<float>(i0);
    return channel.freeze[i0] + (channel.freeze[i1] - channel.freeze[i0]) * frac;
}

void ReTimeEffect::updateTiming(float timeParam, float barsParam)
{
    static constexpr float kRatios[5] = {0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
    static constexpr float kBars[4] = {0.5f, 1.0f, 2.0f, 4.0f};

    const float t = juce::jlimit(0.0f, 1.0f, timeParam);
    const float bp = juce::jlimit(0.0f, 1.0f, barsParam);
    const int ratioIdx = juce::jlimit(0, 4, static_cast<int>(std::lround(t * 4.0f)));
    const int barsIdx = juce::jlimit(0, 3, static_cast<int>(std::lround(bp * 3.0f)));

    const float newSpeed = kRatios[ratioIdx];
    const float newLength = static_cast<float>(m_sampleRate * 60.0 / m_bpm * (kBars[barsIdx] * 4.0));
    const float maxLength = m_channels.empty() ? newLength
                                                : static_cast<float>(m_channels.front().ring.size() - 2);
    const float clampedLength = juce::jlimit(64.0f, std::max(64.0f, maxLength), newLength);

    if (newSpeed != m_speed || std::abs(clampedLength - m_loopLength) > 2.0f)
    {
        m_speed = newSpeed;
        m_loopLength = clampedLength;
        m_needsSync = true;
    }
}

void ReTimeEffect::recapture()
{
    if (m_channels.empty())
        return;

    const float sr = static_cast<float>(m_sampleRate);
    const float samplesPerBeat = sr * 60.0f / m_bpm;

    float endDelay = 0.0f;
    if (m_hasTransport)
    {
        const double ppq = std::max(0.0, m_ppqPosition);
        const double barStart = std::floor(ppq / 4.0) * 4.0;
        endDelay = static_cast<float>((ppq - barStart) * samplesPerBeat);
    }

    for (auto& channel : m_channels)
    {
        const float ringSize = static_cast<float>(channel.ring.size());
        const float endPos = static_cast<float>(channel.writePos) - endDelay;
        const float startPos = endPos - m_loopLength;
        const size_t loopLen = static_cast<size_t>(m_loopLength);
        for (size_t i = 0; i < loopLen; ++i)
        {
            const float pos = wrapPosition(startPos + static_cast<float>(i), ringSize);
            channel.freeze[i] = channel.ring[static_cast<size_t>(pos)];
        }
    }

    m_phase = wrapPosition(m_shift * m_loopLength, m_loopLength);
    m_needsSync = false;
    m_hasCaptured = true;
}

void ReTimeEffect::processBlock(float** buffer, int numChannels, int numSamples, const float* params)
{
    juce::ScopedNoDenormals noDenormals;
    if (m_channels.empty() || numSamples <= 0)
        return;

    updateTiming(params[1], params[2]);
    m_shift = juce::jlimit(0.0f, 1.0f, params[3]);

    if (m_needsSync || !m_hasCaptured)
        recapture();

    const int channels = std::min(numChannels, static_cast<int>(m_channels.size()));
    const float loopLength = m_loopLength;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& channel = m_channels[static_cast<size_t>(ch)];
            const float input = std::isfinite(buffer[ch][sample]) ? buffer[ch][sample] : 0.0f;
            channel.ring[channel.writePos] = input;

            const float raw = readInterpolated(channel, m_phase);
            channel.smoothState += (raw - channel.smoothState) * m_smoothAlpha;
            buffer[ch][sample] = channel.smoothState;
            peak = std::max(peak, std::abs(channel.smoothState));
            channel.writePos = (channel.writePos + 1) % channel.ring.size();
        }

        m_phase += m_speed;
        if (m_phase >= loopLength)
        {
            m_phase -= loopLength;
            if (m_isPlaying || !m_hasTransport)
                recapture();
        }
        m_hasTail = peak > 1.0e-8f;
    }
}

void ReTimeEffect::processSample(float** buffer, int numChannels, int sampleNum, float driveParam)
{
    (void)driveParam;
    const float params[4] = {1.0f, 0.5f, 0.5f, 0.0f};
    processBlock(buffer, numChannels, sampleNum + 1, params);
}
