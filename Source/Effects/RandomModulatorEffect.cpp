#include <JuceHeader.h>
#include "Effects/RandomModulatorEffect.h"

#include <cmath>

void RandomModulatorEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch.holdValue = 0.0f;
        ch.current = 0.0f;
        ch.counter = 0;
    }
}

void RandomModulatorEffect::reset()
{
    for (auto& ch : m_channels)
    {
        ch.holdValue = 0.0f;
        ch.current = 0.0f;
        ch.counter = 0;
    }
}

void RandomModulatorEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float rate = effectParam;
    float updateRateHz = 0.1f + rate * 19.9f;
    int updateInterval = static_cast<int>(m_sampleRate / updateRateHz);
    if (updateInterval < 1) updateInterval = 1;

    auto xorshift32 = [](uint32_t& state) -> float {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0xFFFFFF) / 16777215.0f * 2.0f - 1.0f;
    };

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];

        if (mc.counter <= 0)
        {
            mc.holdValue = xorshift32(m_rngState) * m_depth;
            mc.counter = updateInterval + (ch * 7) % updateInterval;
        }
        --mc.counter;

        float smooth = mc.current + m_smoothFactor * (mc.holdValue - mc.current);
        if (std::abs(smooth - mc.holdValue) < 0.001f)
            smooth = mc.holdValue;
        mc.current = smooth;
        b[ch][s] *= mc.current;
    }
}

void RandomModulatorEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    m_depth = params[0];
    float smooth = params[1];
    float rate = params[2];

    m_smoothFactor = 1.0f - std::exp(-2.0f * 3.14159265f * (1.0f + smooth * 19.0f) / static_cast<float>(m_sampleRate));

    float updateRateHz = 0.1f + rate * 19.9f;
    int updateInterval = static_cast<int>(m_sampleRate / updateRateHz);
    if (updateInterval < 1) updateInterval = 1;

    auto xorshift32 = [](uint32_t& state) -> float {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0xFFFFFF) / 16777215.0f * 2.0f - 1.0f;
    };

    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& mc = m_channels[static_cast<size_t>(ch)];

        if (mc.counter <= 0)
        {
            mc.holdValue = xorshift32(m_rngState) * m_depth;
            mc.counter = updateInterval + (ch * 7) % updateInterval;
        }
        --mc.counter;

        mc.current += m_smoothFactor * (mc.holdValue - mc.current);
        if (std::abs(mc.current - mc.holdValue) < 1e-4f)
            mc.current = mc.holdValue;

        float mod = mc.current * 0.5f + 0.5f;
        for (int s = 0; s < n; ++s)
            b[ch][s] *= mod;
    }
}
