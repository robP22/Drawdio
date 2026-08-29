#pragma once
#include <vector>
#include "Effects/DspEffect.h"

class SpectralFilterEffect : public DspEffect
{
public:
    SpectralFilterEffect() : DspEffect(3) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    std::vector<float> m_bqZ1;
    std::vector<float> m_bqZ2;
    float m_prevCenter = 0.5f;
    float m_prevWidth = 0.5f;
    float m_prevQ = 0.5f;
};
