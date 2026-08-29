#pragma once
#include <vector>
#include "Effects/DspEffect.h"

class VcaCompressorEffect : public DspEffect
{
public:
    VcaCompressorEffect() : DspEffect(1) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    float m_envelopeFollower = 0.0f;
    float m_attackCoeff = 0.0f;
    float m_releaseCoeff = 0.0f;
    float m_makeupGain = 1.0f;
};

class SidechainEffect : public DspEffect
{
public:
    SidechainEffect() : DspEffect(3) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void setTransport(float bpm, double ppqPosition, bool isPlaying) override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    int m_phase = 0;
    float m_smoothEnv = 1.0f;
    float m_smoothCoeff = 0.0f;
    float m_bpm = 120.0f;
};
