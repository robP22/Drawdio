#pragma once
#include <vector>
#include "Dsp/DelayPrimitives.h"
#include "Effects/DspEffect.h"

class DelayEffect : public DspEffect
{
public:
    DelayEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    bool hasActiveTail() const override { return m_hasTail; }
    double getTailLength() const override { return 2.5; }

private:
    std::vector<SimpleDelayState> m_delays;
    std::vector<float> m_fbLpState;
    float m_smoothedDelaySamples = 4410.0f;
    bool m_firstBlock = true;
    bool m_hasTail = false;
};

#include "Effects/GranularBaseEffect.h"

class GranularDelayEffect : public GranularBaseEffect
{
public:
    GranularDelayEffect() : GranularBaseEffect(0.15f, 2.0, 0, 3) {}
    void processBlock(float** b, int c, int n, const float* params) override;
};
