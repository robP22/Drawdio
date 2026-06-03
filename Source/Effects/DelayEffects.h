#pragma once
#include <vector>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class ModulatedDelayEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    SimpleDelayState m_delay;
    float m_lfoPhase = 0.0f;
};

class SimpleDelayEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    SimpleDelayState m_delay;
};

class DynamicRingBufferEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    RingBufferState m_buffer;
};

class TapeStopEchoEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<float> m_buf;
    size_t m_writePtr = 0;
    float m_readHead = 0.0f;
};

#include "Effects/GranularBaseEffect.h"

class GranularDelayEffect : public GranularBaseEffect
{
public:
    GranularDelayEffect() : GranularBaseEffect(0.15f, 2.0) {}
};
