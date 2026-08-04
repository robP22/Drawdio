#pragma once
#include <vector>
#include "Dsp/DelayPrimitives.h"
#include "Effects/DspEffect.h"

class MicroPitchChorusEffect : public DspEffect
{
public:
    MicroPitchChorusEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct MicropitchState {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos1 = 0.0f;
        float readPos2 = 0.0f;
        float lfoPhase = 0.0f;
    };
    std::vector<MicropitchState> m_channels;
    float m_depth = 0.5f;
    float m_lfoRate = 0.3f;
};

class SimpleDelayEffect : public DspEffect
{
public:
    SimpleDelayEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    std::vector<SimpleDelayState> m_delays;
    std::vector<float> m_fbLpState;
    std::vector<float> m_lfoPhase;
    float m_smoothedDelaySamples = 4410.0f;
};

class TapeStopEchoEffect : public DspEffect
{
public:
    TapeStopEchoEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct TapeStopChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos = 0.0f;
        float readSpeed = 1.0f;
        bool wasBraking = false;
        float wowPhase = 0.0f;
        int brakeXfadePos = 32;
        float brakeXfadeOldOutput = 0.0f;
    };
    std::vector<TapeStopChannel> m_channels;
    float m_predelayMs = 100.0f;
};

#include "Effects/GranularBaseEffect.h"

class GranularDelayEffect : public GranularBaseEffect
{
public:
    GranularDelayEffect() : GranularBaseEffect(0.15f, 2.0, 0) {}
};
