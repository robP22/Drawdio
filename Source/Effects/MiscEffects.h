#pragma once
#include <vector>
#include "Effects/DspEffect.h"

class VcaCompressorEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    int mixKnobIndex() const override { return -1; }

private:
    float m_envelopeFollower = 0.0f;
    float m_attackCoeff = 0.0f;
    float m_releaseCoeff = 0.0f;
    float m_makeupGain = 1.0f;
};

class SidechainDuckerEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    int mixKnobIndex() const override { return -1; }

private:
    struct SidechainChannel {
        int timer = 0;
        int intervalSamples = 0;
    };
    std::vector<SidechainChannel> m_channels;
    float m_duckAmount = 0.0f;
};
