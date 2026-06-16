#include <JuceHeader.h>
#include "Effects/PitchEffects.h"

#include <algorithm>
#include <cmath>

void FrequencyShifterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_phase = 0.0f;
    m_allpassZ1.assign(static_cast<size_t>(numChannels), 0.0f);
    m_allpassZ2.assign(static_cast<size_t>(numChannels), 0.0f);
}

void FrequencyShifterEffect::reset()
{
    m_phase = 0.0f;
    std::fill(m_allpassZ1.begin(), m_allpassZ1.end(), 0.0f);
    std::fill(m_allpassZ2.begin(), m_allpassZ2.end(), 0.0f);
}

void FrequencyShifterEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float shift = effectParam;
    float shiftHz = shift * shift * 2000.0f;

    m_phase += static_cast<float>(shiftHz / m_sampleRate);
    if (m_phase >= 1.0f) m_phase -= 1.0f;

    float cosPhi = std::cos(m_phase * 2.0f * 3.14159265f);
    float sinPhi = std::sin(m_phase * 2.0f * 3.14159265f);

    float w1 = static_cast<float>(3.14159265f * (300.0f + shiftHz * 0.3f) / m_sampleRate);
    float tanHalf1 = std::tan(w1);
    float a1 = (tanHalf1 - 1.0f) / (tanHalf1 + 1.0f);

    float w2 = static_cast<float>(3.14159265f * (1200.0f + shiftHz * 0.3f) / m_sampleRate);
    float tanHalf2 = std::tan(w2);
    float a2 = (tanHalf2 - 1.0f) / (tanHalf2 + 1.0f);

    int chCount = std::min(c, static_cast<int>(m_allpassZ1.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float x = b[ch][s];
        float& z1 = m_allpassZ1[static_cast<size_t>(ch)];
        float q1 = a1 * x + z1;
        z1 = x - a1 * q1;

        float& z2 = m_allpassZ2[static_cast<size_t>(ch)];
        float q2 = a2 * x + z2;
        z2 = x - a2 * q2;

        float q = (q1 + q2) * 0.5f;
        float shifted = x * cosPhi + q * sinPhi;
        b[ch][s] = shifted;
    }
}

void GlitchStutterEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_states.resize(static_cast<size_t>(numChannels));
    for (auto& s : m_states)
    {
        s.buf.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
        s.writePtr = 0;
        s.sliceCounter = 0;
        s.playCounter = 0;
        s.repeatCount = 0;
        s.sliceStart = 0;
        s.sliceLen = 0;
        s.gateCounter = 0;
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

        gs.buf[gs.writePtr] = b[ch][s];

        size_t sliceSamples = static_cast<size_t>(m_sampleRate * sliceLenSec + 0.5f);
        if (sliceSamples < 2) sliceSamples = 2;
        size_t gateSamples = sliceSamples / 4;
        if (gateSamples < 1) gateSamples = 1;

        if (gs.mode == GlitchState::RECORDING)
        {
            gs.sliceCounter++;
            b[ch][s] = gs.buf[gs.writePtr];

            if (gs.sliceCounter >= static_cast<int>(sliceSamples))
            {
                gs.sliceStart = (gs.writePtr + bufSize - sliceSamples) % bufSize;
                gs.sliceLen = sliceSamples;
                gs.playCounter = 0;
                gs.repeatCount = 0;
                gs.mode = GlitchState::PLAYING;
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
                float fadeIn = static_cast<float>(offset) / static_cast<float>(kXfadeLen);
                float fadeOut = 1.0f - fadeIn;
                size_t curPos = (gs.sliceStart + static_cast<size_t>(gs.playCounter)) % bufSize;
                size_t wrapPos = (gs.sliceStart + static_cast<size_t>(offset)) % bufSize;
                b[ch][s] = gs.buf[curPos] * fadeOut + gs.buf[wrapPos] * fadeIn;
            }
            else
            {
                size_t pos = (gs.sliceStart + static_cast<size_t>(gs.playCounter)) % bufSize;
                b[ch][s] = gs.buf[pos];
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
                }
            }
        }
        else
        {
            b[ch][s] = 0.0f;
            gs.gateCounter++;
            if (gs.gateCounter >= static_cast<int>(gateSamples))
            {
                gs.mode = GlitchState::RECORDING;
                gs.sliceCounter = 0;
            }
        }

        gs.writePtr = (gs.writePtr + 1) % bufSize;
    }
}
