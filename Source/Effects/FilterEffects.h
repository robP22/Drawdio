#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
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

class AllpassCascadeEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<std::vector<float>> m_delays;
};

class FormantShifterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<float> m_lpState;
    float m_envState = 0.0f;
};
