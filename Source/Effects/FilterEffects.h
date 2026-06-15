#pragma once
#include <vector>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class BiquadFilterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<float> m_lpState;
};

class SpectralFreezeEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    struct FreezeChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos = 0.0f;
        float lfoPhase = 0.0f;
        size_t freezeLen = 0;
        bool wasFrozen = false;
    };
    std::vector<FreezeChannel> m_channels;
};

class FormantShifterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<float> m_lp1;
    std::vector<float> m_lp2;
    float m_envState = 0.0f;
};
