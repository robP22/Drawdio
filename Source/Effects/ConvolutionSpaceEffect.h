#pragma once
#include <vector>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class ConvolutionSpaceEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    int mixKnobIndex() const override { return 0; }

private:
    struct IrChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        std::vector<float> ir;
        size_t irLen = 0;
    };
    std::vector<IrChannel> m_channels;
    size_t m_currentPreset = 0;
};
