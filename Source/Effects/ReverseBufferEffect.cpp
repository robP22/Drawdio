#include <JuceHeader.h>
#include "Effects/ReverseBufferEffect.h"
#include <algorithm>
#include <cmath>

void ReverseBufferEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 1.5), 0.0f);
        ch.writePtr = 0;
        ch.mode = RevState::RECORDING;
        ch.sliceStart = 0;
        ch.sliceLen = 0;
        ch.playPos = 0;
        ch.sliceCounter = 0;
        ch.repeatCount = 0;
        ch.xfadePos = RevChannel::kXfadeLen;
    }
}

void ReverseBufferEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.mode = RevState::RECORDING;
        ch.sliceStart = 0;
        ch.sliceLen = 0;
        ch.playPos = 0;
        ch.sliceCounter = 0;
        ch.repeatCount = 0;
        ch.xfadePos = RevChannel::kXfadeLen;
    }
}

void ReverseBufferEffect::processSample(float** b, int c, int s, float effectParam)
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
        if (sliceSamples >= bufSize) sliceSamples = bufSize - 1;

        if (rc.mode == RevState::RECORDING)
        {
            int fadeOutStart = static_cast<int>(sliceSamples) - RevChannel::kXfadeLen;
            if (fadeOutStart < 0) fadeOutStart = 0;

            if (rc.sliceCounter >= fadeOutStart)
            {
                int offset = rc.sliceCounter - fadeOutStart;
                float fade = 1.0f - static_cast<float>(offset) / static_cast<float>(RevChannel::kXfadeLen);
                b[ch][s] *= fade;
            }

            rc.sliceCounter++;
            if (rc.sliceCounter >= static_cast<int>(sliceSamples))
            {
                rc.sliceStart = (rc.writePtr + bufSize - sliceSamples) % bufSize;
                rc.sliceLen = sliceSamples;
                rc.playPos = 0;
                rc.repeatCount = 0;
                rc.xfadePos = 0;
                rc.mode = RevState::PLAYING;
                rc.sliceCounter = 0;
            }
        }
        else
        {
            size_t readPos = (rc.sliceStart + rc.sliceLen - 1 - static_cast<size_t>(rc.playPos)) % bufSize;

            if (rc.xfadePos < RevChannel::kXfadeLen)
            {
                float a = static_cast<float>(rc.xfadePos) / static_cast<float>(RevChannel::kXfadeLen);
                float w = 0.5f * (1.0f - std::cos(3.14159265f * a));
                b[ch][s] = rc.buf[readPos] * w;
                rc.xfadePos++;
            }
            else
            {
                b[ch][s] = rc.buf[readPos];
            }

            rc.playPos++;
            if (rc.playPos >= static_cast<int>(rc.sliceLen))
            {
                rc.playPos = 0;
                rc.xfadePos = 0;
                rc.repeatCount++;
                if (rc.repeatCount >= maxRepeats)
                {
                    rc.mode = RevState::RECORDING;
                    rc.sliceCounter = 0;
                    rc.xfadePos = RevChannel::kXfadeLen;
                }
            }
        }

        rc.writePtr = (rc.writePtr + 1) % bufSize;
    }
}

void ReverseBufferEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[3]);
}
