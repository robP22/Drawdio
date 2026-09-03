#pragma once
#include <memory>
#include <vector>
#include "Effects/DspEffect.h"

struct ConvolutionChannel;

class ConvolutionReverbEffect : public DspEffect
{
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kBlockLen = kFftSize / 2;

    ConvolutionReverbEffect();
    ~ConvolutionReverbEffect() override;
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    bool hasActiveTail() const override { return m_hasTail; }
    double getTailLength() const override { return 1.5; }

private:
    void precomputePartitionSpectra();

    std::vector<std::unique_ptr<ConvolutionChannel>> m_channels;
    std::vector<std::vector<float>> m_corrSpectra;
    std::vector<std::vector<float>> m_decorrSpectra;
    std::vector<float> m_scales;
    int m_partitionCount = 0;
    float m_dampCutoff = -1.0f;
    float m_dampB0 = 1.0f, m_dampB1 = 0.0f, m_dampB2 = 0.0f;
    float m_dampA1 = 0.0f, m_dampA2 = 0.0f;
    float m_dampB0Prev = 1.0f, m_dampB1Prev = 0.0f, m_dampB2Prev = 0.0f;
    float m_dampA1Prev = 0.0f, m_dampA2Prev = 0.0f;
    bool m_hasTail = false;
};
