#pragma once
#include <vector>
#include <atomic>
#include <array>
#include <cstdint>
#include "PedalStructures.h"
#include "Effects/DspEffect.h"

class UnifiedPedalProcessor
{
public:
    UnifiedPedalProcessor();
    ~UnifiedPedalProcessor();

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock, int numChannels = 2);
    void reset();
    void loadPedalConfiguration(const PedalAssetPayload* config);
    void processAudioBlock(float** buffer, int numChannels, int numSamples);
    std::vector<ParameterDescriptor> getCurrentParams() const;
    const PedalAssetPayload* getCurrentConfig() const;

    void updateParameter(int physicalSlot, int knobIdx, float newValue);
    void storeParameterValue(int physicalSlot, int knobIdx, float value);
    void applyParamOffset(int physicalSlot, int knobIdx, float dragStartValue, float newValue);
    void clearParamOffsets();
    float getKnobDisplayValue(int slot, int knob, float compiledValue) const;
    void invalidateParamCacheForSlot(int physicalSlot);

    void drainReleaseQueue();
    void tryApplyDeferredConfig();
    bool hasPendingReleases() const;
    void scheduleReset();
    bool isParamOverridden(int physicalSlot, int knobIdx) const;
    uint32_t getParamOverrideMask() const { return m_paramCacheValidMask.load(std::memory_order_acquire); }

    struct ParameterSnapshot
    {
        std::array<float, 24> values;
        uint32_t revision;
    };
    ParameterSnapshot getSnapshot() const;

private:
    void processChainBlock(float** b, int c, int s, const PedalAssetPayload& config,
                           std::array<std::unique_ptr<DspEffect>, PedalSlotCount>& effects);
    float readParam(uint16_t token, float fallback,
                    const PedalAssetPayload& config, uint8_t nodeIndex) const;

    void pushToReleaseQueue(const PedalAssetPayload* ptr);

    std::atomic<double> m_sampleRate{44100.0};
    std::atomic<int> m_maxSamplesPerBlock{1024};
    std::atomic<int> m_maxChannels{2};
    std::atomic<int> m_crossfadeSamples{882};

    std::atomic<uint32_t> m_paramRevision{0};
    std::array<std::atomic<float>, 24> m_parameterCache;
    std::atomic<uint32_t> m_paramCacheValidMask{0};
    std::array<float, 24> m_paramOffsets{};

    std::atomic<const PedalAssetPayload*> m_currentConfig{nullptr};
    std::atomic<const PedalAssetPayload*> m_nextConfig{nullptr};
    std::atomic<const PedalAssetPayload*> m_deferredConfig{nullptr};
    std::atomic<int> m_crossfadeCounter{0};
    std::atomic<bool> m_pendingReset{false};
    std::atomic<bool> m_pendingCrossfadeReset{false};

    static constexpr int kReleaseQueueCapacity = 16;
    std::array<const PedalAssetPayload*, kReleaseQueueCapacity> m_releaseQueue{};
    std::atomic<int> m_releaseWriteIndex{0};
    std::atomic<int> m_releaseReadIndex{0};

    static constexpr float kCrossfadeMs = 20.0f;

    std::vector<std::vector<float>> m_dryBuffer;
    std::vector<std::vector<float>> m_crossfadeTempBuf;
    std::vector<std::vector<float>> m_crossfadeOldOut;

    static std::unique_ptr<DspEffect> createEffectForType(DspModuleType type);

    std::array<std::unique_ptr<DspEffect>, PedalSlotCount> m_chainEffects;
    std::array<std::unique_ptr<DspEffect>, PedalSlotCount> m_pendingEffects;
    std::array<std::atomic<uint8_t>, PedalSlotCount> m_chainEffectTypes{};
    std::atomic<const PedalAssetPayload*> m_audioReleasePtr{nullptr};
    void prebuildEffects(const PedalAssetPayload* config, bool& deferred);

    int m_silentBlockCount = 0;
};
