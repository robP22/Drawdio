#pragma once
#include <vector>
#include <atomic>
#include <array>
#include <cstdint>
#include <memory>
#include "Core/CompiledPedalConfig.h"
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"
#include "Effects/DspEffect.h"
#include "State/ParameterCache.h"
#include "State/CompiledParameterBank.h"
#include "State/PedalState.h"
#include "State/CrossfadeState.h"

struct ConfigAudioView;

class UnifiedPedalProcessor
{
public:
    UnifiedPedalProcessor();
    ~UnifiedPedalProcessor();

    void prepareToPlay(double sampleRate, int maxSamplesPerBlock, int numChannels = 2);
    void reset(const PedalAssetPayload& config);
    void processAudioBlock(float** buffer, int numChannels, int numSamples,
                           ConfigAudioView& cfg);
    void setKnobParameter(int physicalSlot, int knobIdx, float dragStartValue, float newValue);
    void scheduleReset();
    void crossfadeReset() { m_crossfade.requestReset(); }

    void updateParameter(int physicalSlot, int knobIdx, float newValue) { m_paramCache.update(physicalSlot, knobIdx, newValue); }
    void storeParameterValue(int physicalSlot, int knobIdx, float value) { m_paramCache.store(physicalSlot, knobIdx, value); }
    void setCompiledParameterValue(int physicalSlot, int knobIdx, float value) { m_compiledParameterBank.store(physicalSlot, knobIdx, value); }
    float getCompiledParameterValue(int physicalSlot, int knobIdx) const { return m_compiledParameterBank.load(physicalSlot, knobIdx); }
    void clearParamOffsets() { m_paramCache.clearOffsets(); }
    float getKnobDisplayValue(int slot, int knob, float compiledValue) const { return m_paramCache.getKnobDisplayValue(slot, knob, compiledValue); }
    void invalidateParamCacheForSlot(int physicalSlot) { m_paramCache.invalidateSlot(physicalSlot); }
    bool isParamOverridden(int physicalSlot, int knobIdx) const { return m_paramCache.isOverridden(physicalSlot, knobIdx); }
    uint32_t getParamOverrideMask() const { return m_paramCache.getOverrideMask(); }
    ParameterCache::Snapshot getSnapshot() const { return m_paramCache.getSnapshot(); }

    void setDriftAmount(int physicalSlot, float amount)
    {
        if (physicalSlot >= 0 && physicalSlot < PedalSlotCount)
            m_driftAmounts[static_cast<size_t>(physicalSlot)].store(std::max(0.0f, std::min(1.0f, amount)), std::memory_order_release);
    }
    float getDriftAmount(int physicalSlot) const
    {
        if (physicalSlot < 0 || physicalSlot >= PedalSlotCount)
            return 0.0f;
        return m_driftAmounts[static_cast<size_t>(physicalSlot)].load(std::memory_order_acquire);
    }

    PedalState& pedalState() { return m_pedalState; }
    const PedalState& pedalState() const { return m_pedalState; }
    void setAutomationValue(float val) { m_currentAutomationValue.store(val, std::memory_order_relaxed); }
    void setTransport(float bpm, double ppqPosition, bool isPlaying)
    {
        m_transportBpm.store(bpm, std::memory_order_relaxed);
        m_transportPpq.store(ppqPosition, std::memory_order_relaxed);
        m_transportPlaying.store(isPlaying, std::memory_order_relaxed);
    }

    double getSampleRate() const { return m_sampleRate.load(std::memory_order_relaxed); }
    int getMaxChannels() const { return m_maxChannels.load(std::memory_order_relaxed); }

private:
    void processChainBlock(float** b, int c, int s, const PedalAssetPayload& config,
                           bool useCompiledParameterBank);

    std::atomic<double> m_sampleRate{44100.0};
    std::atomic<int> m_maxSamplesPerBlock{1024};
    std::atomic<int> m_maxChannels{2};
    ParameterCache m_paramCache;
    CompiledParameterBank m_compiledParameterBank;

    std::atomic<bool> m_pendingReset{false};
    CrossfadeState m_crossfade;

    std::vector<std::vector<float>> m_dryBuffer;

    PedalState m_pedalState;
    std::atomic<float> m_currentAutomationValue{0.0f};
    std::atomic<float> m_transportBpm{120.0f};
    std::atomic<double> m_transportPpq{0.0};
    std::atomic<bool> m_transportPlaying{false};
    float m_smoothedAutoValue = 0.0f;
    float m_paramSmoothHz = 40.0f;
    float m_paramSmoothAlphaMaxBlock = 0.0f;
    float m_paramSmoothAlpha = 0.0f;
    std::array<std::array<float, KnobsPerPedal>, PedalSlotCount> m_smoothedParams = {
        std::array<float, KnobsPerPedal>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, KnobsPerPedal>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, KnobsPerPedal>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, KnobsPerPedal>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, KnobsPerPedal>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, KnobsPerPedal>{0.5f, 0.5f, 0.5f, 0.5f}
    };
    std::array<float, PedalSlotCount> m_prevMix = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};

    struct DriftModulator {
        uint32_t rngState = 0x12345678u;
        float phase = 0.0f;
        float value = 0.0f;
        float target = 0.0f;
        float phaseUnstable = 0.0f;
        float valueUnstable = 0.0f;
        float targetUnstable = 0.0f;
    };
    std::array<DriftModulator, PedalSlotCount> m_drift = {
        DriftModulator{0x12345678u + 0u * 0x9E3779B9u},
        DriftModulator{0x12345678u + 1u * 0x9E3779B9u},
        DriftModulator{0x12345678u + 2u * 0x9E3779B9u},
        DriftModulator{0x12345678u + 3u * 0x9E3779B9u},
        DriftModulator{0x12345678u + 4u * 0x9E3779B9u},
        DriftModulator{0x12345678u + 5u * 0x9E3779B9u}
    };
    std::array<std::atomic<float>, PedalSlotCount> m_driftAmounts = {
        std::atomic<float>{0.0f}, std::atomic<float>{0.0f}, std::atomic<float>{0.0f},
        std::atomic<float>{0.0f}, std::atomic<float>{0.0f}, std::atomic<float>{0.0f}
    };
};
