#pragma once
#include "Effects/DspEffect.h"
#include <array>

class PitchShifterEffect : public DspEffect
{
public:
    PitchShifterEffect() : DspEffect(0) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct ChannelState
    {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos = 0.0f;
        float readPos2 = 0.0f;
        int fadePos = 0;
        bool fading = false;
    };
    std::vector<ChannelState> m_channels;
    int m_xfadeLen = 1;
    float m_initDelaySamples = 0.0f;
    float m_maxGapSamples = 0.0f;
    float m_speed = 1.0f;
};

class FrequencyShifterEffect : public DspEffect
{
public:
    FrequencyShifterEffect() : DspEffect(1) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    float m_phase = 0.0f;
    struct FreqShiftChannel {
        static constexpr int kSections = 4;
        std::array<float, kSections> inputA{};
        std::array<float, kSections> outputA{};
        std::array<float, kSections> inputB{};
        std::array<float, kSections> outputB{};
    };
    std::vector<FreqShiftChannel> m_channels;
};

class GlitchStutterEffect : public DspEffect
{
public:
    GlitchStutterEffect() : DspEffect(1) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct GlitchState {
        std::vector<float> buf;
        std::vector<float> freeze;
        size_t writePtr = 0;
        int sliceCounter = 0;
        int playCounter = 0;
        int repeatCount = 0;
        size_t sliceStart = 0;
        size_t sliceLen = 0;
        int gateCounter = 0;
        int gateFadeIn = 0;
        size_t exitReadPos = 0;
        int exitFade = 0;
        int entryXfadePos = 32;
        float entryXfadeFrom = 0.0f;
        enum Mode { RECORDING, PLAYING, GATED };
        Mode mode = RECORDING;
    };
    std::vector<GlitchState> m_states;
    uint32_t m_rng = 0x9E3779B9u;
    float m_randomAmount = 0.0f;
    int m_xfadeLen = 32;
};
