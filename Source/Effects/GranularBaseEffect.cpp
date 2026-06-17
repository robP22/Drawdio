#include <JuceHeader.h>
#include "Effects/GranularBaseEffect.h"

GranularBaseEffect::GranularBaseEffect(float grainDurationSec, float durationSec)
    : m_grainDurationSec(grainDurationSec), m_durationSec(durationSec)
{
}

void GranularBaseEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_states.resize(static_cast<size_t>(numChannels));
    for (auto& s : m_states)
        prepareGranularProcessor(s, sampleRate, static_cast<double>(m_durationSec));
}

void GranularBaseEffect::reset()
{
    for (auto& s : m_states)
        resetGranularProcessor(s);
}

void GranularBaseEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float rate = effectParam;
    float playbackSpeed = std::exp2(rate * 2.0f - 1.0f);

    int chCount = std::min(c, static_cast<int>(m_states.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        float out = processGranularSample(b[ch][s], m_states[static_cast<size_t>(ch)],
                                          playbackSpeed, m_sampleRate, m_grainDurationSec);
        b[ch][s] = out;
    }
}

void GranularBaseEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float playbackSpeed = std::exp2(params[3] * 2.0f - 1.0f);

    int chCount = std::min(c, static_cast<int>(m_states.size()));
    for (int ch = 0; ch < chCount; ++ch)
        for (int s = 0; s < n; ++s)
            b[ch][s] = processGranularSample(b[ch][s], m_states[static_cast<size_t>(ch)],
                                              playbackSpeed, m_sampleRate, m_grainDurationSec);
}
