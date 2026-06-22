#pragma once
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class GranularBaseEffect : public DspEffect
{
public:
    GranularBaseEffect(float grainDurationSec, float durationSec = 2.0);
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

protected:
    float m_grainDurationSec;
    float m_durationSec;
    std::vector<GranularProcessorState> m_states;
};
