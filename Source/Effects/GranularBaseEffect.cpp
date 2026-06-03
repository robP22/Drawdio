#include "Effects/GranularBaseEffect.h"

GranularBaseEffect::GranularBaseEffect(float grainDurationSec, float durationSec)
    : m_grainDurationSec(grainDurationSec), m_durationSec(durationSec)
{
}

void GranularBaseEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    prepareGranularProcessor(m_state, sampleRate, static_cast<double>(m_durationSec));
}

void GranularBaseEffect::reset()
{
    resetGranularProcessor(m_state);
}

void GranularBaseEffect::processSample(float** b, int c, int s, float effectParam)
{
    float pitch = effectParam;
    float pitchRatio = 0.5f + pitch * 1.5f;

    float input = (c > 0) ? b[0][s] : 0.0f;
    float out = processGranularSample(input, m_state, pitchRatio, m_sampleRate, m_grainDurationSec);

    for (int ch = 0; ch < c; ++ch)
        b[ch][s] = out;
}
