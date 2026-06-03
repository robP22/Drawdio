#pragma once
#include <vector>
#include <atomic>
#include <memory>
#include <array>
#include <cstdint>
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

    // Lock-free parameter snapshot for audio thread
    struct ParameterSnapshot
    {
        std::array<float, 24> values;  // 6 slots × 4 params
        uint32_t revision;
    };
    ParameterSnapshot getSnapshot() const;

private:
    void processWithConfig(float** b, int c, int s, const PedalAssetPayload& config);
    float readParam(uint16_t token, float fallback) const;

    double m_sampleRate;
    int m_maxSamplesPerBlock;
    int m_maxChannels;
    int m_crossfadeSamples;  // Calculated from kCrossfadeMs and sampleRate

    // Atomic parameter access - UI writes, DSP reads
    std::atomic<uint32_t> m_paramRevision{0};
    std::array<std::atomic<float>, 24> m_parameterCache;

    std::atomic<std::shared_ptr<PedalAssetPayload>> m_currentConfig{nullptr};
    std::atomic<std::shared_ptr<PedalAssetPayload>> m_nextConfig{nullptr};
    int m_crossfadeCounter;
    int m_currentNodeIndex;

    const PedalAssetPayload* m_activeConfig = nullptr;

    // Crossfade time in milliseconds (consistent across sample rates)
    // 20ms crossfade: 44.1kHz → 882 samples, 96kHz → 1920 samples, 192kHz → 3840 samples
    static constexpr float kCrossfadeMs = 20.0f;

    // Preallocated buffers - no allocations during processBlock
    std::vector<float> m_dryBuffer;
    std::vector<std::vector<float>> m_crossfadeTempBuf;
    std::vector<float> m_crossfadeOldOut;

    // Effect registry indexed by DspModuleType.
    std::array<std::unique_ptr<DspEffect>, 19> m_effects;
};
