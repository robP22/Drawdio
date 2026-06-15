#pragma once
#include <vector>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class WaveshaperEffect : public DspEffect
{
public:
    void prepare(double, int) override {}
    void reset() override {}
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, float effectParam) override;
};

class WavefolderEffect : public DspEffect
{
public:
    void prepare(double, int) override {}
    void reset() override {}
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, float effectParam) override;
};

class CombResonatorEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<SimpleDelayState> m_delays;
};
