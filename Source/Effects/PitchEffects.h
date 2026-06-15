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
    std::vector<float> m_allpassZ;
};

class GlitchStutterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    struct GlitchState {
        std::vector<float> buf;
        size_t writePtr = 0;
        int sliceCounter = 0;
        int playCounter = 0;
        int repeatCount = 0;
        size_t sliceStart = 0;
        size_t sliceLen = 0;
        int gateCounter = 0;
        enum Mode { RECORDING, PLAYING, GATED };
        Mode mode = RECORDING;
    };
    std::vector<GlitchState> m_states;
};
