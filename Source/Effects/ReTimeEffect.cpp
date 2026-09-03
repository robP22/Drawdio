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
    }
    m_fadeFrom.assign(m_channels.size(), 0.0f);
    m_lastOut.assign(m_channels.size(), 0.0f);
    m_releaseFrom.assign(m_channels.size(), 0.0f);

    m_loopLength = static_cast<float>(sampleRate * 2.0);
    m_phase = 0.0f;
    m_speed = 0.5f;
    m_xfadePos = 0;
    m_xfadeLen = 0;
    m_historySamples = 0;
    m_releasePos = 0;
    m_releaseLength = std::max<size_t>(1, static_cast<size_t>(sampleRate * kReleaseSeconds));
    m_silenceSamples = 0;
    m_needsSync = true;
    m_hasCaptured = false;
    m_hasTail = false;
    m_state = PlaybackState::Priming;
    m_previousPpqValid = false;
}

void ReTimeEffect::reset()
{
    for (auto& channel : m_channels)
    {
        std::fill(channel.ring.begin(), channel.ring.end(), 0.0f);
        channel.writePos = 0;
    }
    std::fill(m_fadeFrom.begin(), m_fadeFrom.end(), 0.0f);
    std::fill(m_lastOut.begin(), m_lastOut.end(), 0.0f);
    std::fill(m_releaseFrom.begin(), m_releaseFrom.end(), 0.0f);

    m_phase = 0.0f;
    m_xfadePos = 0;
    m_xfadeLen = 0;
    m_historySamples = 0;
    m_releasePos = 0;
    m_silenceSamples = 0;
    m_needsSync = true;
    m_hasCaptured = false;
    m_hasTail = false;
    m_state = PlaybackState::Priming;
    m_previousPpqValid = false;
}

void ReTimeEffect::setTransport(float bpm, double ppqPosition, bool isPlaying)
{
    const float safeBpm = std::isfinite(bpm) && bpm > 1.0f ? bpm : 120.0f;

    const double safePpq = std::isfinite(ppqPosition) ? ppqPosition : 0.0;

    if (isPlaying && !m_isPlaying)
    {
        m_needsSync = true;
        m_hasCaptured = false;
        m_historySamples = 0;
        m_silenceSamples = 0;
        m_state = PlaybackState::Priming;
        m_previousPpqValid = false;
    }
    else if (!isPlaying && m_isPlaying)
    {
        startRelease();
    }
    else if (isPlaying && m_isPlaying && m_previousPpqValid
             && (safePpq < m_previousPpqPosition - 0.25
                 || safePpq > m_previousPpqPosition + 1.0))
    {
        m_needsSync = true;
    }

    m_bpm = safeBpm;
    m_ppqPosition = safePpq;
    m_isPlaying = isPlaying;
    m_hasTransport = true;
    m_previousPpqPosition = safePpq;
    m_previousPpqValid = isPlaying;
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

float ReTimeEffect::readInterpolated(const std::vector<float>& buffer, float position) const
{
    const float size = static_cast<float>(buffer.size());
    if (size <= 1.0f)
        return 0.0f;

    const float wrapped = wrapPosition(position, size);
    const auto i0 = static_cast<size_t>(wrapped);
    const auto i1 = (i0 + 1) % buffer.size();
    const float frac = wrapped - static_cast<float>(i0);
    return buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
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
    const size_t xfadeLen = static_cast<size_t>(m_sampleRate * 0.040);
    m_xfadeLen = std::max<size_t>(1, std::min<size_t>(xfadeLen, std::max<size_t>(1, static_cast<size_t>(m_loopLength / 2.0f))));
    m_xfadePos = 0;
    m_needsSync = false;
    m_hasCaptured = true;
    m_state = PlaybackState::Captured;
    m_releasePos = 0;
    m_silenceSamples = 0;
}

void ReTimeEffect::startRelease()
{
    if (m_state == PlaybackState::Releasing || m_state == PlaybackState::Idle)
        return;

    for (size_t ch = 0; ch < m_releaseFrom.size(); ++ch)
        m_releaseFrom[ch] = m_lastOut[ch];
    m_releasePos = 0;
    m_releaseLength = std::max<size_t>(1, static_cast<size_t>(m_sampleRate * kReleaseSeconds));
    m_state = PlaybackState::Releasing;
}

size_t ReTimeEffect::requiredHistorySamples() const
{
    if (m_channels.empty())
        return 0;

    float endDelay = 0.0f;
    if (m_hasTransport)
    {
        const double ppq = std::max(0.0, m_ppqPosition);
        const double barStart = std::floor(ppq / 4.0) * 4.0;
        endDelay = static_cast<float>((ppq - barStart) * (m_sampleRate * 60.0 / m_bpm));
    }

    const auto required = static_cast<size_t>(std::ceil(m_loopLength + endDelay));
    return std::min(required, m_channels.front().ring.size());
}

bool ReTimeEffect::hasSufficientHistory() const
{
    return m_historySamples >= requiredHistorySamples();
}

void ReTimeEffect::processBlock(float** buffer, int numChannels, int numSamples, const float* params)
{
    juce::ScopedNoDenormals noDenormals;
    if (m_channels.empty() || numSamples <= 0)
        return;

    updateTiming(params[1], params[2]);
    m_shift = juce::jlimit(0.0f, 1.0f, params[3]);

    if ((m_needsSync || !m_hasCaptured)
        && m_state != PlaybackState::Releasing
        && m_state != PlaybackState::Idle)
    {
        if (hasSufficientHistory())
        {
            for (size_t ch = 0; ch < m_channels.size(); ++ch)
                m_fadeFrom[ch] = m_lastOut[ch];
            recapture();
        }
        else
        {
            m_state = PlaybackState::Priming;
            m_hasCaptured = false;
        }
    }

    const int channels = std::min(numChannels, static_cast<int>(m_channels.size()));
    const float loopLength = m_loopLength;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float peak = 0.0f;
        float inputActivity = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float input = std::isfinite(buffer[ch][sample]) ? buffer[ch][sample] : 0.0f;
            inputActivity = std::max(inputActivity, std::abs(input));
        }

        if ((m_state == PlaybackState::Releasing || m_state == PlaybackState::Idle)
            && inputActivity > kInputSilenceThreshold
            && (m_isPlaying || !m_hasTransport))
        {
            m_state = PlaybackState::Priming;
            m_needsSync = true;
            m_hasCaptured = false;
            m_historySamples = 0;
            m_releasePos = 0;
            m_silenceSamples = 0;
        }

        const bool xfading = m_state == PlaybackState::Captured && m_xfadePos < m_xfadeLen;
        float xfadeMix = 1.0f;
        if (xfading)
        {
            const float t = static_cast<float>(m_xfadePos) / static_cast<float>(m_xfadeLen);
            xfadeMix = 0.5f - 0.5f * std::cos(3.14159265358979323846f * t);
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            auto& channel = m_channels[static_cast<size_t>(ch)];
            const float input = std::isfinite(buffer[ch][sample]) ? buffer[ch][sample] : 0.0f;

            channel.ring[channel.writePos] = input;

            float out = 0.0f;
            if (m_state == PlaybackState::Priming)
            {
                out = input;
            }
            else if (m_state == PlaybackState::Captured)
            {
                const float raw = readInterpolated(channel.freeze, m_phase);
                out = raw;
                if (xfading)
                    out = m_fadeFrom[static_cast<size_t>(ch)]
                        + (raw - m_fadeFrom[static_cast<size_t>(ch)]) * xfadeMix;
            }
            else if (m_state == PlaybackState::Releasing)
            {
                const float t = static_cast<float>(m_releasePos)
                              / static_cast<float>(std::max<size_t>(1, m_releaseLength));
                const float releaseMix = 0.5f + 0.5f * std::cos(3.14159265358979323846f * t);
                out = m_releaseFrom[static_cast<size_t>(ch)] * releaseMix;
            }

            buffer[ch][sample] = out;
            m_lastOut[static_cast<size_t>(ch)] = out;
            peak = std::max(peak, std::abs(out));
            channel.writePos = (channel.writePos + 1) % channel.ring.size();
        }

        if (m_state != PlaybackState::Idle)
            m_historySamples = std::min(m_historySamples + 1, m_channels.front().ring.size());

        if (m_state == PlaybackState::Priming && m_needsSync && hasSufficientHistory())
        {
            for (size_t ch = 0; ch < m_channels.size(); ++ch)
                m_fadeFrom[ch] = m_lastOut[ch];
            recapture();
        }

        if (inputActivity > kInputSilenceThreshold)
            m_silenceSamples = 0;
        else
            m_silenceSamples = std::min(m_silenceSamples + 1,
                                        static_cast<size_t>(m_sampleRate * kInputSilenceTimeoutSeconds));

        if (m_state == PlaybackState::Captured
            && m_silenceSamples >= static_cast<size_t>(m_sampleRate * kInputSilenceTimeoutSeconds)
            && (m_isPlaying || !m_hasTransport))
        {
            startRelease();
        }

        if (xfading)
            ++m_xfadePos;

        if (m_state == PlaybackState::Captured)
        {
            m_phase += m_speed;
            if (m_phase >= loopLength)
            {
                m_phase -= loopLength;
                for (int ch = 0; ch < channels; ++ch)
                    m_fadeFrom[static_cast<size_t>(ch)] = m_lastOut[static_cast<size_t>(ch)];
                if ((m_isPlaying || !m_hasTransport) && hasSufficientHistory())
                    recapture();
                else
                    m_xfadePos = 0;
            }
        }

        if (m_state == PlaybackState::Releasing)
        {
            ++m_releasePos;
            if (m_releasePos >= m_releaseLength)
            {
                m_state = PlaybackState::Idle;
                m_hasCaptured = false;
                m_hasTail = false;
                m_xfadePos = 0;
            }
        }
        m_hasTail = m_state != PlaybackState::Idle && peak > 1.0e-8f;
    }
}

void ReTimeEffect::processSample(float** buffer, int numChannels, int sampleNum, float driveParam)
{
    (void)driveParam;
    const float params[4] = {1.0f, 0.5f, 0.5f, 0.0f};
    processBlock(buffer, numChannels, sampleNum + 1, params);
}
