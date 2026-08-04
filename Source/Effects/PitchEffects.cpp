#include <JuceHeader.h>
#include "Effects/PitchEffects.h"

#include <algorithm>
#include <cmath>

void FrequencyShifterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_phase = 0.0f;
    m_channels.assign(static_cast<size_t>(numChannels), FreqShiftChannel{});
}

void FrequencyShifterEffect::reset()
{
    m_phase = 0.0f;
    m_channels.assign(m_channels.size(), FreqShiftChannel{});
}

void FrequencyShifterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float shift = effectParam;
    float shiftHz = shift * shift * 2000.0f;
    float sr = static_cast<float>(m_sampleRate);

    m_phase += shiftHz / sr;
    if (m_phase >= 1.0f) m_phase -= 1.0f;

    float cosPhi = std::cos(m_phase * 6.2831853f);
    float sinPhi = std::sin(m_phase * 6.2831853f);

    static constexpr float kA1a = 0.47940086f, kA1b = 0.87621849f, kA1c = 0.97659735f;
    static constexpr float kA2a = 0.16175849f, kA2b = 0.73302892f, kA2c = 0.94534970f;

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float x = b[ch][s];
        if (!std::isfinite(x)) x = 0.0f;
        auto& chState = m_channels[static_cast<size_t>(ch)];

        float q1a = kA1a * x + chState.z1a;
        chState.z1a = x - kA1a * q1a;
        float q1b = kA1b * q1a + chState.z1b;
        chState.z1b = q1a - kA1b * q1b;
        float q1c = kA1c * q1b + chState.z1c;
        chState.z1c = q1b - kA1c * q1c;

        float q2a = kA2a * x + chState.z2a;
        chState.z2a = x - kA2a * q2a;
        float q2b = kA2b * q2a + chState.z2b;
        chState.z2b = q2a - kA2b * q2b;
        float q2c = kA2c * q2b + chState.z2c;
        chState.z2c = q2b - kA2c * q2c;

        float q = (q1c + q2c) * 0.5f;
        b[ch][s] = (x * cosPhi + q * sinPhi) * 0.707f;
    }
}

void GlitchStutterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_states.resize(static_cast<size_t>(numChannels));
    for (auto& s : m_states)
    {
        s.buf.assign(static_cast<size_t>(sampleRate * 1.0), 0.0f);
        s.writePtr = 0;
        s.sliceCounter = 0;
        s.playCounter = 0;
        s.repeatCount = 0;
        s.sliceStart = 0;
        s.sliceLen = 0;
        s.gateCounter = 0;
        s.gateFadeIn = 0;
        s.gateFadeOut = 0;
        s.mode = GlitchState::RECORDING;
    }
}

void GlitchStutterEffect::reset()
{
    for (auto& s : m_states)
    {
        std::fill(s.buf.begin(), s.buf.end(), 0.0f);
        s.writePtr = 0;
        s.sliceCounter = 0;
        s.playCounter = 0;
        s.repeatCount = 0;
        s.sliceStart = 0;
        s.sliceLen = 0;
        s.gateCounter = 0;
        s.gateFadeIn = 0;
        s.gateFadeOut = 0;
        s.mode = GlitchState::RECORDING;
    }
}

void GlitchStutterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float sliceLenSec = 0.05f + effectParam * 0.45f;
    int maxRepeats = 1 + static_cast<int>((1.0f - effectParam) * 4.0f);
    static constexpr int kXfadeLen = 32;

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

        if (gs.mode == GlitchState::RECORDING)
        {
            // Fade-in from silence after GATED ended
            if (gs.gateFadeIn > 0)
            {
                float fade = 1.0f - static_cast<float>(--gs.gateFadeIn) / static_cast<float>(kXfadeLen);
                b[ch][s] = gs.buf[gs.writePtr] * fade;
            }
            else
            {
                b[ch][s] = gs.buf[gs.writePtr];
            }

            gs.sliceCounter++;

            if (gs.sliceCounter >= static_cast<int>(sliceSamples))
            {
                gs.sliceStart = (gs.writePtr + bufSize - sliceSamples) % bufSize;
                gs.sliceLen = sliceSamples;
                gs.playCounter = 0;
                gs.repeatCount = 0;
                gs.mode = GlitchState::PLAYING;
                gs.entryXfadePos = 0;
                gs.sliceCounter = 0;
            }
        }
        else if (gs.mode == GlitchState::PLAYING)
        {
            int sliceEnd = static_cast<int>(gs.sliceLen);
            int xfadeStart = sliceEnd - kXfadeLen;
            if (xfadeStart < 0) xfadeStart = 0;

            if (gs.playCounter >= xfadeStart && gs.playCounter < sliceEnd)
            {
                int offset = gs.playCounter - xfadeStart;
                float fadeIn  = 0.5f * (1.0f - std::cos(3.14159265f * static_cast<float>(offset) / static_cast<float>(kXfadeLen)));
                float fadeOut = 0.5f * (1.0f + std::cos(3.14159265f * static_cast<float>(offset) / static_cast<float>(kXfadeLen)));
                size_t curPos = (gs.sliceStart + static_cast<size_t>(gs.playCounter)) % bufSize;
                size_t wrapPos = (gs.sliceStart + static_cast<size_t>(offset)) % bufSize;
                b[ch][s] = gs.buf[curPos] * fadeOut + gs.buf[wrapPos] * fadeIn;
            }
            else
            {
                size_t pos = (gs.sliceStart + static_cast<size_t>(gs.playCounter)) % bufSize;
                float out = gs.buf[pos];
                if (gs.entryXfadePos < GlitchState::kXfadeLen)
                {
                    float w = static_cast<float>(gs.entryXfadePos) / static_cast<float>(GlitchState::kXfadeLen);
                    out *= w;
                    gs.entryXfadePos++;
                }
                b[ch][s] = out;
            }
            gs.playCounter++;

            if (gs.playCounter >= sliceEnd)
            {
                gs.playCounter = 0;
                gs.repeatCount++;
                if (gs.repeatCount >= maxRepeats)
                {
                    gs.mode = GlitchState::GATED;
                    gs.gateCounter = 0;
                    gs.gateFadeOut = kXfadeLen;
                }
            }
        }
        else
        {
            // Fade-out to silence when entering GATED
            if (gs.gateFadeOut > 0)
            {
                float fade = static_cast<float>(--gs.gateFadeOut) / static_cast<float>(kXfadeLen);
                b[ch][s] *= fade;
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
                gs.gateFadeIn = kXfadeLen;
                gs.sliceCounter = 0;
            }
        }

        gs.writePtr = (gs.writePtr + 1) % bufSize;
    }
}

void GlitchStutterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[0]);
}

void FrequencyShifterEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[0]);
}
