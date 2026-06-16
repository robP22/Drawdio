#pragma once
#include <vector>
#include "Effects/DspEffect.h"

class VcaCompressorEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void setVolumeParam(float vol) override;

private:
    float m_envelopeFollower = 0.0f;
    float m_attackMs = 2.0f;
    float m_attackCoeff = 0.0f;
    float m_releaseCoeff = 0.0f;
};

class SampleRateDegraderEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    int m_sampleHold = 0;
    std::vector<float> m_heldValues;
};

class SidechainDuckerEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void setVolumeParam(float vol) override;

private:
    struct SidechainChannel {
        int timer = 0;
        int intervalSamples = 0;
    };
    std::vector<SidechainChannel> m_channels;
    float m_duckAmount = 0.0f;
};
