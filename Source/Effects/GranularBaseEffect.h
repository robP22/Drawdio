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

private:
    float m_grainDurationSec;
    float m_durationSec;
    GranularProcessorState m_state;
};
