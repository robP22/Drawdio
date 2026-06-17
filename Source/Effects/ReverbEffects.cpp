#include "Effects/ReverbEffects.h"
#include "EffectConfigRegistry.h"

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

void ReverbNetworkEffect::processBlock(float** b, int c, int n, const float* params)
{
    for (int s = 0; s < n; ++s)
        processSample(b, c, s, params[3]);
}

DiffusedReverbEffect::DiffusedReverbEffect() : ReverbNetworkEffect(EffectConfigRegistry::getDiffusedReverbConfig()) {}
PlateReverbEffect::PlateReverbEffect() : ReverbNetworkEffect(EffectConfigRegistry::getPlateReverbConfig()) {}
