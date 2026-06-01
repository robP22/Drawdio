#pragma once
#include "Effects/DspEffect.h"

class VcaCompressorEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    float m_envelopeFollower = 0.0f;
};

class SampleRateDegraderEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    int m_sampleHold = 0;
    float m_heldValue = 0.0f;
};
