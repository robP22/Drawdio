#pragma once
#include <vector>
#include "Effects/DspEffect.h"
#include "PedalStructures.h"

class ReverseBufferEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    int mixKnobIndex() const override { return 0; }

private:
    std::vector<SimpleDelayState> m_buffers;
    size_t m_prevWindowSamps = 0;
    float m_xfadeCounter = 32.0f;
    static constexpr float kXfadeLen = 32.0f;
};
