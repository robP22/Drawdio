#pragma once
#include <cmath>
#include "Effects/DspEffect.h"

class WaveshaperEffect : public DspEffect
{
public:
    void prepare(double, int) override {}
    void reset() override {}
    void processSample(float** b, int c, int s, float effectParam) override;
};

class SoftDistortionEffect : public DspEffect
{
public:
    void prepare(double, int) override {}
    void reset() override {}
    void processSample(float** b, int c, int s, float effectParam) override;
};

class WavefolderEffect : public DspEffect
{
public:
    void prepare(double, int) override {}
    void reset() override {}
    void processSample(float** b, int c, int s, float effectParam) override;
};
