#include <JuceHeader.h>
#include "Effects/ReverseEffect.h"
#include <algorithm>
#include <cmath>

void ReverseEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    const size_t freezeSize = static_cast<size_t>(sampleRate);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 1.5), 0.0f);
        ch.freeze.assign(freezeSize, 0.0f);
        ch.writePtr = 0;
        ch.mode = RevState::RECORDING;
        ch.sliceStart = 0;
        ch.sliceLen = 0;
        ch.playPos = 0;
        ch.sliceCounter = 0;
        ch.repeatCount = 0;
        ch.xfadePos = RevChannel::kXfadeLen;
        ch.exitFadePos = RevChannel::kXfadeLen;
    }
}

void ReverseEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        std::fill(ch.freeze.begin(), ch.freeze.end(), 0.0f);
        ch.writePtr = 0;
        ch.mode = RevState::RECORDING;
        ch.sliceStart = 0;
        ch.sliceLen = 0;
        ch.playPos = 0;
        ch.sliceCounter = 0;
        ch.repeatCount = 0;
        ch.xfadePos = RevChannel::kXfadeLen;
        ch.exitFadePos = RevChannel::kXfadeLen;
    }
}

void ReverseEffect::processSample(float** b, int c, int s, float effectParam)
{
    float density = effectParam;
    float grainSec = 0.05f + density * 0.95f;
    int maxRepeats = 1 + static_cast<int>((1.0f - density) * 4.0f);

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& rc = m_channels[static_cast<size_t>(ch)];
        size_t bufSize = rc.buf.size();
        if (bufSize == 0) continue;

        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        rc.buf[rc.writePtr] = in;

        size_t sliceSamples = static_cast<size_t>(m_sampleRate * grainSec + 0.5f);
        if (sliceSamples < 2) sliceSamples = 2;
        if (sliceSamples >= rc.freeze.size()) sliceSamples = rc.freeze.size() - 1;
        if (sliceSamples >= rc.buf.size()) sliceSamples = rc.buf.size() - 1;

        if (rc.mode == RevState::RECORDING)
        {
            if (rc.exitFadePos < RevChannel::kXfadeLen)
            {
                float a = static_cast<float>(rc.exitFadePos) / static_cast<float>(RevChannel::kXfadeLen);
                float w = 0.5f * (1.0f - std::cos(3.14159265f * a));
                b[ch][s] = rc.exitFadeFrom * (1.0f - w) + in * w;
                rc.exitFadePos++;
            }
            rc.sliceCounter++;
            if (rc.sliceCounter >= static_cast<int>(sliceSamples))
            {
                rc.sliceStart = (rc.writePtr + rc.buf.size() - sliceSamples) % rc.buf.size();
                rc.sliceLen = sliceSamples;
                for (size_t i = 0; i < sliceSamples; ++i)
                {
                    const size_t pos = (rc.sliceStart + i) % rc.buf.size();
                    rc.freeze[i] = rc.buf[pos];
                }
                rc.playPos = 0;
                rc.repeatCount = 0;
                rc.xfadePos = 0;
                rc.xfadeFrom = b[ch][s];
                rc.mode = RevState::PLAYING;
                rc.sliceCounter = 0;
            }
        }
        else
        {
            size_t readPos = rc.sliceLen - 1 - static_cast<size_t>(rc.playPos);

            if (rc.xfadePos < RevChannel::kXfadeLen)
            {
                float a = static_cast<float>(rc.xfadePos) / static_cast<float>(RevChannel::kXfadeLen);
                float w = 0.5f * (1.0f - std::cos(3.14159265f * a));
                b[ch][s] = rc.xfadeFrom * (1.0f - w) + rc.freeze[readPos] * w;
                rc.xfadePos++;
            }
            else
            {
                b[ch][s] = rc.freeze[readPos];
            }

            rc.playPos++;
            if (rc.playPos >= static_cast<int>(rc.sliceLen))
            {
                rc.playPos = 0;
                rc.xfadePos = 0;
                rc.xfadeFrom = b[ch][s];
                rc.repeatCount++;
                if (rc.repeatCount >= maxRepeats)
                {
                    rc.mode = RevState::RECORDING;
                    rc.sliceCounter = 0;
                    rc.xfadePos = RevChannel::kXfadeLen;
                    rc.exitFadePos = 0;
                    rc.exitFadeFrom = b[ch][s];
                }
            }
        }

        rc.writePtr = (rc.writePtr + 1) % bufSize;
    }
}

void ReverseEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[3]);
}
