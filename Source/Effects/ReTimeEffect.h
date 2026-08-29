#pragma once

#include "Effects/DspEffect.h"
#include <vector>

class ReTimeEffect final : public DspEffect
{
public:
    ReTimeEffect() : DspEffect(0) {}

    void prepare(double sampleRate, int numChannels) override;
    void reset() override;
    void setTransport(float bpm, double ppqPosition, bool isPlaying) override;
    void processSample(float** buffer, int numChannels, int sampleNum, float driveParam) override;
    void processBlock(float** buffer, int numChannels, int numSamples, const float* params) override;
    bool hasActiveTail() const override { return m_hasTail; }
    double getTailLength() const override { return 16.0; }

private:
    struct ChannelState
    {
        std::vector<float> ring;
        std::vector<float> freeze;
        size_t writePos = 0;
        float smoothState = 0.0f;
    };

    void updateTiming(float timeParam, float barsParam);
    void recapture();
    float readInterpolated(const ChannelState& channel, float position) const;
    static float wrapPosition(float position, float size);

    std::vector<ChannelState> m_channels;
    float m_loopLength = 44100.0f;
    float m_phase = 0.0f;
    float m_speed = 0.5f;
    float m_shift = 0.0f;
    float m_smoothAlpha = 0.5f;
    float m_bpm = 120.0f;
    double m_ppqPosition = 0.0;
    bool m_isPlaying = false;
    bool m_hasTransport = false;
    bool m_needsSync = true;
    bool m_hasCaptured = false;
    bool m_hasTail = false;
};
