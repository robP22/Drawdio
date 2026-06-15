#pragma once
#include <vector>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class MicroPitchChorusEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    struct MicropitchState {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos1 = 0.0f;
        float readPos2 = 0.0f;
        float lfoPhase = 0.0f;
    };
    std::vector<MicropitchState> m_channels;
};

class SimpleDelayEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<SimpleDelayState> m_delays;
};

class DynamicRingBufferEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    std::vector<RingBufferState> m_buffers;
};

class TapeStopEchoEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    struct TapeStopChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos = 0.0f;
        float readSpeed = 1.0f;
    };
    std::vector<TapeStopChannel> m_channels;
};

#include "Effects/GranularBaseEffect.h"

class GranularDelayEffect : public GranularBaseEffect
{
public:
    GranularDelayEffect() : GranularBaseEffect(0.15f, 2.0) {}
};
