#pragma once
#include <vector>
#include <cstdint>
#include "Effects/DspEffect.h"

class BitcrusherEffect : public DspEffect
{
public:
    BitcrusherEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct BitcrusherChannel {
        double phase = 0.0;
        float hold = 0.0f;
        float prevInput = 0.0f;
        float lpZ1 = 0.0f;
        float lpZ2 = 0.0f;
        float dcZ1 = 0.0f;
        float dcPrevIn = 0.0f;
        uint32_t rngState = 0;
    };
    std::vector<BitcrusherChannel> m_channels;
    float m_lpB0 = 1.0f, m_lpB1 = 0.0f, m_lpB2 = 0.0f;
    float m_lpA1 = 0.0f, m_lpA2 = 0.0f;
    float m_lpB0Prev = 1.0f, m_lpB1Prev = 0.0f, m_lpB2Prev = 0.0f;
    float m_lpA1Prev = 0.0f, m_lpA2Prev = 0.0f;
    float m_prevCutoff = -1.0f;
};
