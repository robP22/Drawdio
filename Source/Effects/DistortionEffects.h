#pragma once
#include <vector>
#include "Dsp/DelayPrimitives.h"
#include "Effects/DspEffect.h"

class WaveshaperEffect : public DspEffect
{
public:
    void prepare(double, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    std::vector<float> m_prevSample;
};

class WavefolderEffect : public DspEffect
{
public:
    void prepare(double, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    std::vector<float> m_prevSample;
};

class CombResonatorEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    bool hasActiveTail() const override { return m_hasTail; }
    double getTailLength() const override { return 0.8; }

private:
    std::vector<SimpleDelayState> m_delays;
    std::vector<float> m_dampState;
    float m_dampCoeff = 0.0f;
    bool m_hasTail = false;
};
