#pragma once
#include <cstdint>
#include <vector>
#include "Effects/DspEffect.h"

class RandomModulatorEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct ModChannel {
        float holdValue = 1.0f;
        float current = 1.0f;
        int counter = 0;
    };
    std::vector<ModChannel> m_channels;
    float m_smoothFactor = 0.0f;
    float m_depth = 1.0f;
    uint32_t m_rngState = 12345;
};
