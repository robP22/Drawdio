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
    double getTailLength() const override { return kReleaseSeconds; }

private:
    enum class PlaybackState
    {
        Priming,
        Captured,
        Releasing,
        Idle
    };

    struct ChannelState
    {
        std::vector<float> ring;
        std::vector<float> freeze;
        size_t writePos = 0;
    };

    void updateTiming(float timeParam, float barsParam);
    void recapture();
    void startRelease();
    bool hasSufficientHistory() const;
    size_t requiredHistorySamples() const;
    float readInterpolated(const std::vector<float>& buffer, float position) const;
    static float wrapPosition(float position, float size);

    std::vector<ChannelState> m_channels;
    std::vector<float> m_fadeFrom;
    std::vector<float> m_lastOut;
    std::vector<float> m_releaseFrom;
    float m_loopLength = 44100.0f;
    float m_phase = 0.0f;
    float m_speed = 0.5f;
    float m_shift = 0.0f;
    size_t m_xfadePos = 0;
    size_t m_xfadeLen = 0;
    size_t m_historySamples = 0;
    size_t m_releasePos = 0;
    size_t m_releaseLength = 1;
    size_t m_silenceSamples = 0;
    float m_bpm = 120.0f;
    double m_ppqPosition = 0.0;
    double m_previousPpqPosition = 0.0;
    bool m_isPlaying = false;
    bool m_hasTransport = false;
    bool m_previousPpqValid = false;
    bool m_needsSync = true;
    bool m_hasCaptured = false;
    bool m_hasTail = false;
    PlaybackState m_state = PlaybackState::Priming;

    static constexpr float kInputSilenceThreshold = 1.0e-5f;
    static constexpr double kInputSilenceTimeoutSeconds = 0.5;
    static constexpr double kReleaseSeconds = 0.05;
};
