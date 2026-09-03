#pragma once
#include <vector>
#include "Dsp/DelayPrimitives.h"
#include "Effects/DspEffect.h"

class SpectralFreezeEffect : public DspEffect
{
public:
    SpectralFreezeEffect() : DspEffect(1) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;
    bool hasActiveTail() const override { return true; }
    double getTailLength() const override { return 2.0; }

private:
    struct FreezeChannel {
        std::vector<float> buf;
        std::vector<float> freezeBuf;
        size_t writePtr = 0;
        float readPos = 0.0f;
        size_t freezeLen = 0;
        bool wasFrozen = false;
        bool historyReady = false;
        float entryXfadePos = 32.0f;
        float exitXfadePos = 32.0f;
        float exitXfadeFrom = 0.0f;
        float offsetXfadePos = 32.0f;
        float offsetXfadeFrom = 0.0f;
    };
    std::vector<FreezeChannel> m_channels;
    float m_xfadeLen = 32.0f;
    float m_offset = 0.0f;
    float m_lastOffset = -1.0f;
};

class FormantShifterEffect : public DspEffect
{
public:
    FormantShifterEffect() : DspEffect(0) {}
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
    MultiModeFilterEffect() : DspEffect(1) {}
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, const float* params) override;

private:
    struct SVFState { float lp = 0.0f, bp = 0.0f; };
    std::vector<SVFState> m_states;
    float m_prevCutoffHz = 20.0f;
};
