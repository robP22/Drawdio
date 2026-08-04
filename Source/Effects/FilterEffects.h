#pragma once
#include <vector>
#include "Dsp/DelayPrimitives.h"
#include "Effects/DspEffect.h"

class TimeDomainFreezeEffect : public DspEffect
{
public:
    TimeDomainFreezeEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct FreezeChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos = 0.0f;
        float lfoPhase = 0.0f;
        size_t freezeLen = 0;
        bool wasFrozen = false;
        float xfadePos = 32.0f;
        float oldReadPos = 0.0f;
        float entryXfadePos = 32.0f;
        static constexpr float kXfadeLen = 32.0f;
    };
    std::vector<FreezeChannel> m_channels;
};

class DynamicResonantFilter : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    std::vector<float> m_lp1;
    std::vector<float> m_lp2;
    float m_envState = 0.0f;
    float m_attackCoeff = 0.0f;
    float m_releaseCoeff = 0.0f;
};

class MultiModeFilterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct SVFState { float lp = 0.0f, bp = 0.0f; };
    std::vector<SVFState> m_states;
};
