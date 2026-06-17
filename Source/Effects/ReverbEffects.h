#pragma once
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class ReverbNetworkEffect : public DspEffect
{
public:
    ReverbNetworkEffect(const ReverbNetworkConfig& config);
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    int mixKnobIndex() const override { return 0; }

private:
    ReverbNetworkConfig m_config;
    ReverbNetworkState m_state;
};

class DiffusedReverbEffect : public ReverbNetworkEffect
{
public:
    DiffusedReverbEffect();
};

class PlateReverbEffect : public ReverbNetworkEffect
{
public:
    PlateReverbEffect();
};
