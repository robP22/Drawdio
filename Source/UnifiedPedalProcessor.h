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
    void clearParamOffsets() { m_paramCache.clearOffsets(); }
    float getKnobDisplayValue(int slot, int knob, float compiledValue) const { return m_paramCache.getKnobDisplayValue(slot, knob, compiledValue); }
    void invalidateParamCacheForSlot(int physicalSlot) { m_paramCache.invalidateSlot(physicalSlot); }
    bool isParamOverridden(int physicalSlot, int knobIdx) const { return m_paramCache.isOverridden(physicalSlot, knobIdx); }
    uint32_t getParamOverrideMask() const { return m_paramCache.getOverrideMask(); }
    ParameterCache::Snapshot getSnapshot() const { return m_paramCache.getSnapshot(); }

    PedalState& pedalState() { return m_pedalState; }
    const PedalState& pedalState() const { return m_pedalState; }
    void setAutomationValue(float val) { m_currentAutomationValue.store(val, std::memory_order_relaxed); }

    double getSampleRate() const { return m_sampleRate.load(std::memory_order_relaxed); }
    int getMaxChannels() const { return m_maxChannels.load(std::memory_order_relaxed); }

private:
    void processChainBlock(float** b, int c, int s, const PedalAssetPayload& config);

    std::atomic<double> m_sampleRate{44100.0};
    std::atomic<int> m_maxSamplesPerBlock{1024};
    std::atomic<int> m_maxChannels{2};
    ParameterCache m_paramCache;

    std::atomic<bool> m_pendingReset{false};
    CrossfadeState m_crossfade;

    std::vector<std::vector<float>> m_dryBuffer;

    PedalState m_pedalState;
    std::atomic<float> m_currentAutomationValue{0.0f};
    float m_smoothedAutoValue = 0.0f;
    float m_autoSmoothAlpha = 0.0f;
    float m_paramSmoothAlpha = 0.0f;
    std::array<std::array<float, 4>, PedalSlotCount> m_smoothedParams = {
        std::array<float, 4>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, 4>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, 4>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, 4>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, 4>{0.5f, 0.5f, 0.5f, 0.5f},
        std::array<float, 4>{0.5f, 0.5f, 0.5f, 0.5f}
    };
    std::array<float, PedalSlotCount> m_prevMix = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
};
