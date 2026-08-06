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
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    float m_phase = 0.0f;
    struct FreqShiftChannel {
        float z1a = 0, z1b = 0, z1c = 0;
        float z2a = 0, z2b = 0, z2c = 0;
    };
    std::vector<FreqShiftChannel> m_channels;
};

class GlitchStutterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct GlitchState {
        static constexpr int kXfadeLen = 32;
        std::vector<float> buf;
        size_t writePtr = 0;
        int sliceCounter = 0;
        int playCounter = 0;
        int repeatCount = 0;
        size_t sliceStart = 0;
        size_t sliceLen = 0;
        int gateCounter = 0;
        int gateFadeIn = 0;
        int gateFadeOut = 0;
        int entryXfadePos = kXfadeLen;
        float entryXfadeFrom = 0.0f;
        enum Mode { RECORDING, PLAYING, GATED };
        Mode mode = RECORDING;
    };
    std::vector<GlitchState> m_states;
};
