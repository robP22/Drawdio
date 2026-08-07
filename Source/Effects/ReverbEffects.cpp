#include <JuceHeader.h>
#include "Effects/ReverbEffects.h"
#include "State/EffectConfigRegistry.h"

ReverbNetworkEffect::ReverbNetworkEffect(const ReverbNetworkConfig& config, int decayKnobIndex)
    : DspEffect(0), m_config(config), m_decayKnobIndex(decayKnobIndex)
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
    juce::ScopedNoDenormals noDenorm;
    float peak = 0.0f;
    for (int s = 0; s < n; ++s)
    {
        processSample(b, c, s, params[m_decayKnobIndex]);
        peak = std::max(peak, std::max(std::abs(b[0][s]), (c > 1) ? std::abs(b[1][s]) : 0.0f));
    }
    m_hasTail = (peak > 1e-8f);
}

DiffusedReverbEffect::DiffusedReverbEffect() : ReverbNetworkEffect(EffectConfigRegistry::getDiffusedReverbConfig(), 3) {}
PlateReverbEffect::PlateReverbEffect() : ReverbNetworkEffect(EffectConfigRegistry::getPlateReverbConfig(), 2) {}
