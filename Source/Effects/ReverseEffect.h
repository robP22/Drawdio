#pragma once
#include <vector>
#include "Effects/DspEffect.h"
#include "Dsp/DelayPrimitives.h"

class ReverseEffect : public DspEffect
{
public:
    ReverseEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    enum class RevState { RECORDING, PLAYING };

    struct RevChannel {
        std::vector<float> buf;
        std::vector<float> freeze;
        size_t writePtr = 0;
        RevState mode = RevState::RECORDING;
        size_t sliceStart = 0;
        size_t sliceLen = 0;
        int playPos = 0;
        int sliceCounter = 0;
        int repeatCount = 0;
        int xfadePos = 32;
        float xfadeFrom = 0.0f;
        int exitFadePos = 32;
        float exitFadeFrom = 0.0f;
    };
    std::vector<RevChannel> m_channels;
    int m_xfadeLen = 32;
};
