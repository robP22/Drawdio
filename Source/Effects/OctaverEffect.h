#pragma once
#include <vector>
#include "Effects/DspEffect.h"

class OctaverEffect : public DspEffect
{
public:
    OctaverEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct OctaveChannel {
        float prevSign = 1.0f;
        bool flipFlop = false;
        float dcZ1 = 0.0f;
        float dcPrevIn = 0.0f;
        float bpZ1 = 0.0f, bpZ2 = 0.0f;
        float lpZ1 = 0.0f, lpZ2 = 0.0f;
        float toneZ1 = 0.0f, toneZ2 = 0.0f;
    };
    std::vector<OctaveChannel> m_channels;
    float m_bpB0 = 0.0f, m_bpB1 = 0.0f, m_bpB2 = 0.0f, m_bpA1 = 0.0f, m_bpA2 = 0.0f;
    float m_lpB0 = 0.0f, m_lpB1 = 0.0f, m_lpB2 = 0.0f, m_lpA1 = 0.0f, m_lpA2 = 0.0f;
    float m_toneB0 = 0.0f, m_toneB1 = 0.0f, m_toneB2 = 0.0f, m_toneA1 = 0.0f, m_toneA2 = 0.0f;
    float m_prevTone = -1.0f;
};
