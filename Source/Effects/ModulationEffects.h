#pragma once
#include <vector>
#include "Effects/DspEffect.h"
#include "Dsp/DelayPrimitives.h"

class ChorusEffect : public DspEffect
{
public:
    ChorusEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct ChorusChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        float lfoPhase = 0.0f;
    };
    std::vector<ChorusChannel> m_channels;
    float m_centerDelaySamples = 1323.0f;
    float m_maxDepthSamples = 265.0f;
};

class TremoloEffect : public DspEffect
{
public:
    TremoloEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct TremChannel {
        float phase = 0.0f;
        float squareSmooth = 0.0f;
    };
    std::vector<TremChannel> m_channels;
};

class FlangerEffect : public DspEffect
{
public:
    FlangerEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct FlangerChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        float lfoPhase = 0.0f;
        float fbLp = 0.0f;
    };
    std::vector<FlangerChannel> m_channels;
    float m_minDelaySamples = 44.0f;
    float m_maxDelaySamples = 441.0f;
};
