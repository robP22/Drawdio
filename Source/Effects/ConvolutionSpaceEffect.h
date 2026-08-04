#pragma once
#include <memory>
#include <vector>
#include "Effects/DspEffect.h"

struct FftChannel;

class ConvolutionSpaceEffect : public DspEffect
{
public:
    ConvolutionSpaceEffect();
    ~ConvolutionSpaceEffect() override;
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    static constexpr int kFftOrder = 10;
    static constexpr int kFftSize = 1 << kFftOrder;

    void recomputeIrFreq(float damp, size_t chIdx);
    void processBlockBruteForce(float** b, int c, int n, float damp);
    void processSubBlock(float** b, int offset, int subN, size_t chIdx);

    std::vector<std::unique_ptr<FftChannel>> m_channels;
};
