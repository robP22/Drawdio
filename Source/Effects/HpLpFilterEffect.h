#pragma once
#include <vector>
#include "Effects/DspEffect.h"

class HpLpFilterEffect : public DspEffect
{
public:
    HpLpFilterEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct BiquadState { float z1 = 0.0f; float z2 = 0.0f; };
    struct ChannelState { BiquadState hp; BiquadState lp; };
    std::vector<ChannelState> m_channels;
    float m_prevHigh = 0.0f;
    float m_prevLow = 1.0f;
    float m_prevQ = 0.0f;
};
