#include "Effects/ReverbEffects.h"

namespace {

constexpr ReverbNetworkConfig kDiffReverbConfig = {
    0.2, 0.7,
    { 0.8f, 0.7f, 0.6f, 0.5f },
    0.6f,
    { 30, 37, 41, 47 },
    { 5, 2 }
};

constexpr ReverbNetworkConfig kPlateReverbConfig = {
    0.3, 0.6,
    { 0.9f, 0.85f, 0.78f, 0.7f },
    0.7f,
    { 25, 32, 38, 44 },
    { 4, 3 }
};

}

ReverbNetworkEffect::ReverbNetworkEffect(const ReverbNetworkConfig& config)
    : m_config(config)
{
}

void ReverbNetworkEffect::prepare(double sampleRate, int)
{
    prepareReverbNetwork(m_state, sampleRate, m_config);
}

void ReverbNetworkEffect::reset()
{
    resetReverbNetwork(m_state);
}

void ReverbNetworkEffect::processSample(float** b, int c, int s, float effectParam)
{
    float decay = effectParam;

    float dryL = (c > 0) ? b[0][s] : 0.0f;
    float dryR = (c > 1) ? b[1][s] : dryL;

    float wetL, wetR;
    processReverbNetworkSample(dryL, dryR, m_config, m_state, decay, wetL, wetR);

    if (c > 0) b[0][s] = wetL;
    if (c > 1) b[1][s] = wetR;
}

DiffusedReverbEffect::DiffusedReverbEffect() : ReverbNetworkEffect(kDiffReverbConfig) {}
PlateReverbEffect::PlateReverbEffect() : ReverbNetworkEffect(kPlateReverbConfig) {}
