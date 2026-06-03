#pragma once
#include "Effects/DspEffect.h"
#include "Effects/GranularBaseEffect.h"

class GranularPitchEffect : public GranularBaseEffect
{
public:
    GranularPitchEffect() : GranularBaseEffect(0.11f, 1.0) {}
};

class FrequencyShifterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    float m_phase = 0.0f;
    float m_allpassZ[2] = {};
};

class SubSynthEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    float m_phase = 0.0f;
    float m_prevSample = 0.0f;
    int m_zeroCount = 0;
    float m_measuredFreq = 100.0f;
    int m_silenceCounter = 0;
    static constexpr int kGateSamples = 2048;
};
