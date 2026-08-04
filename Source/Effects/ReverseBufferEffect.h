#pragma once
#include <vector>
#include "Effects/DspEffect.h"
#include "Dsp/DelayPrimitives.h"

class ReverseBufferEffect : public DspEffect
{
public:
    ReverseBufferEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    enum class RevState { RECORDING, PLAYING };

    struct RevChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        RevState mode = RevState::RECORDING;
        size_t sliceStart = 0;
        size_t sliceLen = 0;
        int playPos = 0;
        int sliceCounter = 0;
        int repeatCount = 0;
        int xfadePos = 32;
        static constexpr int kXfadeLen = 32;
    };
    std::vector<RevChannel> m_channels;
};
