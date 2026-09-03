#include <JuceHeader.h>
#include "Effects/PitchEffects.h"
#include "Dsp/DelayPrimitives.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr std::array<float, 4> kFrequencyShiftCoeffsA = {
    0.4794008656f, 0.8762184935f, 0.9765975895f, 0.9974992559f
};
constexpr std::array<float, 4> kFrequencyShiftCoeffsB = {
    0.1617584983f, 0.7330289323f, 0.9453497003f, 0.9905991567f
};
}

void PitchShifterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_xfadeLen = std::max(16, static_cast<int>(sampleRate * 0.04));
    m_initDelaySamples = static_cast<float>(sampleRate * 0.30);
    m_maxGapSamples = static_cast<float>(sampleRate * 0.40);
    m_speed = 1.0f;
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.buf.assign(static_cast<size_t>(sampleRate * 1.0), 0.0f);
        ch.writePtr = 0;
        ch.readPos = static_cast<float>(ch.buf.size()) - m_initDelaySamples;
        ch.readPos2 = 0.0f;
        ch.fadePos = m_xfadeLen;
        ch.fading = false;
    }
}

void PitchShifterEffect::reset()
{
    for (auto& ch : m_channels)
    {
        std::fill(ch.buf.begin(), ch.buf.end(), 0.0f);
        ch.writePtr = 0;
        ch.readPos = static_cast<float>(ch.buf.size()) - m_initDelaySamples;
        ch.readPos2 = 0.0f;
        ch.fadePos = m_xfadeLen;
        ch.fading = false;
    }
}

void PitchShifterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float params[4] = {0.5f, 0.5f, effectParam, 0.5f};
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void PitchShifterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float pitch = std::max(0.0f, std::min(1.0f, params[2]));
    m_speed = std::exp2(pitch * 2.0f - 1.0f);
    const float xfadeLenF = static_cast<float>(m_xfadeLen);
    const float triggerLenF = 1.5f * xfadeLenF;

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];
        const float bufSizeF = static_cast<float>(mc.buf.size());
        if (bufSizeF < 2.0f)
            continue;

        for (int s = 0; s < n; ++s)
        {
            const float input = std::isfinite(b[ch][s]) ? b[ch][s] : 0.0f;
            mc.buf[mc.writePtr] = input;
            mc.writePtr = (mc.writePtr + 1) % mc.buf.size();

            mc.readPos += m_speed;
            if (mc.readPos >= bufSizeF)
                mc.readPos -= bufSizeF;
            if (mc.fading)
            {
                mc.readPos2 += m_speed;
                if (mc.readPos2 >= bufSizeF)
                    mc.readPos2 -= bufSizeF;
            }

            float gap = static_cast<float>(mc.writePtr) - mc.readPos;
            if (gap < 0.0f)
                gap += bufSizeF;
            if (!mc.fading && gap < triggerLenF)
            {
                mc.readPos2 = mc.readPos - xfadeLenF;
                if (mc.readPos2 < 0.0f)
                    mc.readPos2 += bufSizeF;
                mc.fadePos = 0;
                mc.fading = true;
            }
            else if (!mc.fading && gap > m_maxGapSamples)
            {
                mc.readPos2 = mc.readPos + xfadeLenF;
                if (mc.readPos2 >= bufSizeF)
                    mc.readPos2 -= bufSizeF;
                mc.fadePos = 0;
                mc.fading = true;
            }

            float out = interpolateDelayRead(mc.buf, mc.readPos);
            if (mc.fading)
            {
                const float t = static_cast<float>(mc.fadePos) / xfadeLenF;
                const float g = 0.5f - 0.5f * std::cos(3.14159265358979323846f * t);
                out = out * (1.0f - g) + interpolateDelayRead(mc.buf, mc.readPos2) * g;
                ++mc.fadePos;
                if (mc.fadePos >= m_xfadeLen)
                {
                    mc.readPos = mc.readPos2;
                    mc.fading = false;
                }
            }
            b[ch][s] = out;
        }
    }
}

void FrequencyShifterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_phase = 0.0f;
    m_channels.assign(static_cast<size_t>(numChannels), FreqShiftChannel{});
}

void FrequencyShifterEffect::reset()
{
    m_phase = 0.0f;
    std::fill(m_channels.begin(), m_channels.end(), FreqShiftChannel{});
}

void FrequencyShifterEffect::processSample(float** b, int c, int s, float effectParam)
{
    const float shift = std::max(0.0f, std::min(1.0f, effectParam));
    const float shiftHz = shift * 2000.0f;
    float sr = static_cast<float>(m_sampleRate);

    m_phase += shiftHz / sr;
    if (m_phase >= 1.0f) m_phase -= 1.0f;

    float cosPhi = std::cos(m_phase * 6.2831853f);
    float sinPhi = std::sin(m_phase * 6.2831853f);

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float x = b[ch][s];
        if (!std::isfinite(x)) x = 0.0f;
        auto& chState = m_channels[static_cast<size_t>(ch)];

        float inA = x;
        for (int i = 0; i < FreqShiftChannel::kSections; ++i)
        {
            const float a = kFrequencyShiftCoeffsA[static_cast<size_t>(i)];
            const auto index = static_cast<size_t>(i);
            const float outA = chState.inputA[index]
                             - a * inA
                             + a * chState.outputA[index];
            chState.inputA[index] = inA;
            chState.outputA[index] = outA;
            inA = outA;
        }
        float inB = x;
        for (int i = 0; i < FreqShiftChannel::kSections; ++i)
        {
            const float a = kFrequencyShiftCoeffsB[static_cast<size_t>(i)];
            const auto index = static_cast<size_t>(i);
            const float outB = chState.inputB[index]
                             - a * inB
                             + a * chState.outputB[index];
            chState.inputB[index] = inB;
            chState.outputB[index] = outB;
            inB = outB;
        }

        b[ch][s] = inA * cosPhi + inB * sinPhi;
    }
}

void GlitchStutterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_states.resize(static_cast<size_t>(numChannels));
    for (auto& s : m_states)
    {
        s.buf.assign(static_cast<size_t>(sampleRate * 1.0), 0.0f);
        s.freeze.assign(static_cast<size_t>(sampleRate * 1.0), 0.0f);
        s.writePtr = 0;
        s.sliceCounter = 0;
        s.playCounter = 0;
        s.repeatCount = 0;
        s.sliceStart = 0;
        s.sliceLen = 0;
        s.gateCounter = 0;
        s.gateFadeIn = 0;
        s.exitReadPos = 0;
        s.exitFade = 0;
        s.entryXfadePos = std::numeric_limits<int>::max();
        s.mode = GlitchState::RECORDING;
    }
    m_rng = 0x9E3779B9u;
    m_randomAmount = 0.0f;
    m_xfadeLen = 32;
}

void GlitchStutterEffect::reset()
{
    for (auto& s : m_states)
    {
        s.writePtr = 0;
        s.sliceCounter = 0;
        s.playCounter = 0;
        s.repeatCount = 0;
        s.sliceStart = 0;
        s.sliceLen = 0;
        s.gateCounter = 0;
        s.gateFadeIn = 0;
        s.exitReadPos = 0;
        s.exitFade = 0;
        s.entryXfadePos = std::numeric_limits<int>::max();
        s.mode = GlitchState::RECORDING;
    }
}

void GlitchStutterEffect::processSample(float** b, int c, int s, float effectParam)
{
    float sliceLenSec = 0.05f + (1.0f - effectParam) * 0.45f;
    int maxRepeats = 1 + static_cast<int>(std::round(effectParam * 4.0f));

    int chCount = std::min(c, static_cast<int>(m_states.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& gs = m_states[static_cast<size_t>(ch)];
        size_t bufSize = gs.buf.size();
        if (bufSize == 0) continue;

        float in = b[ch][s];
        if (!std::isfinite(in)) in = 0.0f;
        gs.buf[gs.writePtr] = in;

        size_t sliceSamples = static_cast<size_t>(m_sampleRate * sliceLenSec + 0.5f);
        if (sliceSamples < 2) sliceSamples = 2;
        size_t gateSamples = sliceSamples / 4;
        if (gateSamples < 1) gateSamples = 1;
        const int xfadeLen = std::max(1, std::min(m_xfadeLen, static_cast<int>(sliceSamples / 4)));

        if (gs.mode == GlitchState::RECORDING)
        {
            // Fade-in from silence after GATED ended
            if (gs.gateFadeIn > 0)
            {
                int pos = xfadeLen - gs.gateFadeIn;
                float fade = 0.5f * (1.0f - std::cos(3.14159265f * static_cast<float>(pos) / static_cast<float>(xfadeLen)));
                --gs.gateFadeIn;
                b[ch][s] = gs.buf[gs.writePtr] * fade;
            }
            else
            {
                b[ch][s] = gs.buf[gs.writePtr];
            }

            gs.sliceCounter++;

            if (gs.sliceCounter >= static_cast<int>(sliceSamples))
            {
                size_t srcStart;
                if (m_randomAmount > 0.001f && bufSize > sliceSamples)
                {
                    m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;
                    srcStart = static_cast<size_t>(m_rng) % (bufSize - sliceSamples);
                }
                else
                {
                    srcStart = (gs.writePtr + bufSize - sliceSamples + 1) % bufSize;
                }
                for (size_t i = 0; i < sliceSamples; ++i)
                    gs.freeze[i] = gs.buf[(srcStart + i) % bufSize];
                gs.sliceStart = 0;
                gs.sliceLen = sliceSamples;
                gs.playCounter = 0;
                gs.repeatCount = 0;
                gs.mode = GlitchState::PLAYING;
                gs.entryXfadePos = 0;
                gs.entryXfadeFrom = gs.buf[gs.writePtr];
                gs.sliceCounter = 0;
            }
        }
        else if (gs.mode == GlitchState::PLAYING)
        {
            int sliceEnd = static_cast<int>(gs.sliceLen);
            int xfadeStart = sliceEnd - xfadeLen;
            if (xfadeStart < 0) xfadeStart = 0;

            if (gs.playCounter >= xfadeStart && gs.playCounter < sliceEnd)
            {
                int offset = gs.playCounter - xfadeStart;
                float fadeIn  = 0.5f * (1.0f - std::cos(3.14159265f * static_cast<float>(offset) / static_cast<float>(xfadeLen)));
                float fadeOut = 0.5f * (1.0f + std::cos(3.14159265f * static_cast<float>(offset) / static_cast<float>(xfadeLen)));
                size_t curPos = gs.sliceStart + static_cast<size_t>(gs.playCounter);
                size_t wrapPos = gs.sliceStart + static_cast<size_t>(offset);
                float out = gs.freeze[curPos] * fadeOut + gs.freeze[wrapPos] * fadeIn;
                if (gs.entryXfadePos < xfadeLen)
                {
                    float w = static_cast<float>(gs.entryXfadePos) / static_cast<float>(xfadeLen);
                    out = gs.entryXfadeFrom * (1.0f - w) + out * w;
                    gs.entryXfadePos++;
                }
                b[ch][s] = out;
            }
            else
            {
                size_t pos = gs.sliceStart + static_cast<size_t>(gs.playCounter);
                float out = gs.freeze[pos];
                if (gs.entryXfadePos < xfadeLen)
                {
                    float w = static_cast<float>(gs.entryXfadePos) / static_cast<float>(xfadeLen);
                    out = gs.entryXfadeFrom * (1.0f - w) + out * w;
                    gs.entryXfadePos++;
                }
                b[ch][s] = out;
            }
            gs.playCounter++;

            if (gs.playCounter >= sliceEnd)
            {
                gs.playCounter = xfadeLen;
                gs.repeatCount++;
                if (gs.repeatCount >= maxRepeats)
                {
                    gs.mode = GlitchState::GATED;
                    gs.gateCounter = 0;
                    gs.exitReadPos = gs.sliceStart + static_cast<size_t>(xfadeLen - 1);
                    gs.exitFade = xfadeLen;
                }
            }
        }
        else
        {
            if (gs.exitFade > 0)
            {
                float played = gs.freeze[gs.exitReadPos];
                gs.exitReadPos++;
                float fade = static_cast<float>(--gs.exitFade) / static_cast<float>(xfadeLen);
                b[ch][s] = played * fade;
                gs.gateCounter = 0;
            }
            else
            {
                b[ch][s] = 0.0f;
                gs.gateCounter++;
            }

            if (gs.gateCounter >= static_cast<int>(gateSamples))
            {
                gs.mode = GlitchState::RECORDING;
                gs.gateFadeIn = xfadeLen;
                gs.sliceCounter = 0;
            }
        }

        gs.writePtr = (gs.writePtr + 1) % bufSize;
    }
}

void GlitchStutterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    m_randomAmount = std::max(0.0f, std::min(1.0f, params[2]));
    const float smooth = std::max(0.0f, std::min(1.0f, params[3]));
    m_xfadeLen = std::max(static_cast<int>(m_sampleRate * 0.015f),
                          static_cast<int>(m_sampleRate * 0.002f * std::pow(40.0f, smooth) + 0.5f));
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[0]);
}

void FrequencyShifterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[0]);
}
