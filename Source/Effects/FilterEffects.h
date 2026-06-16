#pragma once
#include <vector>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class BiquadFilterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, float effectParam) override;
    void setVolumeParam(float vol) override;

private:
    struct BiquadState { float s1 = 0.0f, s2 = 0.0f; };
    std::vector<BiquadState> m_states;
    float m_resonance = 0.0f;
};

class SpectralFreezeEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

private:
    struct FreezeChannel {
        std::vector<float> buf;
        size_t writePtr = 0;
        float readPos = 0.0f;
        float lfoPhase = 0.0f;
        size_t freezeLen = 0;
        bool wasFrozen = false;
    };
    std::vector<FreezeChannel> m_channels;
};

class FormantShifterEffect : public DspEffect
{
public:
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;

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
    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void processSample(float** b, int c, int s, float effectParam) override;
    void processBlock(float** b, int c, int n, float effectParam) override;
    void setVolumeParam(float vol) override { m_volume = std::clamp(vol, 0.0f, 1.0f); }

private:
    struct SVFState { float lp = 0.0f, bp = 0.0f; };
    std::vector<SVFState> m_states;
    float m_volume = 0.5f;
};
