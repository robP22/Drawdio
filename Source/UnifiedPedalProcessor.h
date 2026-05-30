#pragma once
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <array>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class UnifiedPedalProcessor
{
public:
    UnifiedPedalProcessor();
    ~UnifiedPedalProcessor() = default;

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock, int numChannels = 2);
    void reset();
    void loadPedalConfiguration(std::shared_ptr<PedalAssetPayload> config);
    void processAudioBlock(float** buffer, int numChannels, int numSamples);
    std::vector<ParameterDescriptor> getCurrentParams() const;
    std::shared_ptr<PedalAssetPayload> getCurrentConfig() const;

    void updateParameter(int physicalSlot, int knobIdx, float newValue);

private:
    void processWithConfig(float** b, int c, int s, const PedalAssetPayload& config);
    float readParam(uint16_t token, float fallback) const;

    double m_sampleRate;
    int m_maxChannels;

    mutable std::shared_mutex m_dspMutex;
    std::shared_ptr<PedalAssetPayload> m_currentConfig;
    std::shared_ptr<PedalAssetPayload> m_nextConfig;
    int m_crossfadeCounter;
    int m_currentNodeIndex;

    const PedalAssetPayload* m_activeConfig = nullptr;

    static constexpr int kCrossfadeLength = 1024;

    // Per-channel dry buffer sized to m_maxChannels.
    std::vector<float> m_dryBuffer;

    // Crossfade temp buffers sized to m_maxChannels × block length.
    std::vector<std::vector<float>> m_crossfadeTempBuf;

    // Per-sample crossfade scratch buffer (avoids heap alloc on audio thread).
    std::vector<float> m_crossfadeOldOut;

    // Effect registry indexed by DspModuleType.
    std::array<std::unique_ptr<DspEffect>, 19> m_effects;
};
