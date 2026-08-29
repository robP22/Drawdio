#pragma once
#include "Effects/DspEffect.h"
#include "Effects/GranularBaseEffect.h"

class GrainScrubberEffect : public GranularBaseEffect
{
public:
    GrainScrubberEffect() : GranularBaseEffect(0.08f, 2.0, 1, 3) {}
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
};
