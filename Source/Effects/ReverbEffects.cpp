#include <JuceHeader.h>
#include "Effects/ReverbEffects.h"
#include "State/EffectConfigRegistry.h"

ReverbNetworkEffect::ReverbNetworkEffect(const ReverbNetworkConfig& config, int decayKnobIndex,
                                         int sizeKnobIndex)
    : DspEffect(0), m_config(config), m_decayKnobIndex(decayKnobIndex),
      m_sizeKnobIndex(sizeKnobIndex)
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
    processReverbNetworkSample(dryL, dryR, m_config, m_state, decay, 0.65f, wetL, wetR);

    if (c > 0) b[0][s] = wetL;
    if (c > 1) b[1][s] = wetR;
}

void ReverbNetworkEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float decay = params[m_decayKnobIndex];
    float sizeP = params[m_sizeKnobIndex];
    prepareReverbNetworkBlock(m_state, m_config, decay);

    float peak = 0.0f;
    for (int s = 0; s < n; ++s)
    {
        processReverbNetworkSample((c > 0) ? b[0][s] : 0.0f, (c > 1) ? b[1][s] : 0.0f,
                                   m_config, m_state, decay, sizeP, b[0][s], (c > 1) ? b[1][s] : b[0][s]);
        float x = std::max(std::abs(b[0][s]), (c > 1) ? std::abs(b[1][s]) : 0.0f);
        if (!std::isfinite(x))
        {
            if (c > 0) b[0][s] = 0.0f;
            if (c > 1) b[1][s] = 0.0f;
            x = 0.0f;
        }
        peak = std::max(peak, x);
    }
    m_hasTail = (peak > 1e-8f);
}

DiffusedReverbEffect::DiffusedReverbEffect() : ReverbNetworkEffect(EffectConfigRegistry::getDiffusedReverbConfig(), 3, 1) {}
PlateReverbEffect::PlateReverbEffect() : ReverbNetworkEffect(EffectConfigRegistry::getPlateReverbConfig(), 2, 1) {}
