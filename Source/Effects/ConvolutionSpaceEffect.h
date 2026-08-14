#pragma once
#include <memory>
#include <vector>
#include "Effects/DspEffect.h"

struct FftChannel;

class ConvolutionSpaceEffect : public DspEffect
{
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kDampGridSize = 16;

    ConvolutionSpaceEffect();
    ~ConvolutionSpaceEffect() override;
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    bool hasActiveTail() const override { return m_hasTail; }
    double getTailLength() const override { return 1.5; }

private:
    void precomputeDampGrid(size_t chIdx);
    void processBlockBruteForce(float** b, int c, int n, float damp);
    void processSubBlock(float** b, int offset, int subN, size_t chIdx, int gridIdx);

    std::vector<std::unique_ptr<FftChannel>> m_channels;
    bool m_hasTail = false;
};
