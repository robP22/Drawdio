#include <JuceHeader.h>
#include "Effects/GrainScrubberEffect.h"
#include <algorithm>
#include <cmath>

void GrainScrubberEffect::processSample(float** b, int c, int s, float effectParam)
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

void GrainScrubberEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float position = params[0];
    float playbackSpeed = std::exp2(params[3] * 2.0f - 1.0f);

    int chCount = std::min(c, static_cast<int>(m_states.size()));
    for (int ch = 0; ch < chCount; ++ch)
        for (int s = 0; s < n; ++s)
            b[ch][s] = processGranularSample(b[ch][s], m_states[static_cast<size_t>(ch)],
                                              playbackSpeed, m_sampleRate,
                                              m_grainDurationSec, position);
}
