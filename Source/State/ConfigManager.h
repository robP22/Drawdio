#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "Compile/CanvasMessageQueue.h"
#include "Compile/CompilerThread.h"
#include "Compile/PenDebouncer.h"
#include "Core/CompiledPedalConfig.h"
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"
#include "Core/Contracts/ProcessorInterfaces.h"
#include "Core/Contracts/IConfigConsumer.h"
#include "State/StateSerializer.h"
#include "State/ReleaseQueue.h"
#include "Effects/DspEffect.h"
#include "UnifiedPedalProcessor.h"

struct ConfigAudioView
{
    std::atomic<const PedalAssetPayload*>& currentConfig;
    std::atomic<const PedalAssetPayload*>& nextConfig;
    std::array<std::unique_ptr<DspEffect>, PedalSlotCount>& chainEffects;
    std::array<std::unique_ptr<DspEffect>, PedalSlotCount>& pendingEffects;
    ReleaseQueue& releaseQueue;
    std::array<std::array<const float*, 4>, PedalSlotCount>& paramPtrs;
};

class ConfigManager : public IPedalboardModel,
                      public IBottomBarModel,
                      public IConfigConsumer
{
public:
    ConfigManager(UnifiedPedalProcessor& dsp);
    ~ConfigManager() override;

    // --- IPedalboardModel ---
    void setPedalSlot(int slot, DspModuleType type) override;
    DspModuleType getPedalSlot(int slot) const override;
    void setGridData(const std::array<uint8_t, TotalCells>& data);
    const std::array<uint8_t, TotalCells>& getGridData() const override { return m_gridData; }
    void setManualRouting(const std::vector<uint8_t>& routing) override;
    const std::vector<uint8_t>& getManualRouting() const override { return m_manualRouting; }
    std::array<float, PedalSlotCount * 4> getKnobValues() const override { return m_dsp.getSnapshot().values; }
    uint32_t getParamOverrideMask() const { return m_dsp.getParamOverrideMask(); }
    void setKnobParameter(int slot, int knob, float dragStartValue, float newValue) override { m_dsp.setKnobParameter(slot, knob, dragStartValue, newValue); }
    bool isKnobLinked(int slot, int knob) const override { return m_dsp.pedalState().isKnobLinked(slot, knob); }
    void setKnobLink(int slot, int knob, bool linked) override { m_dsp.pedalState().setKnobLink(slot, knob, linked, 1.0f); }
    float getPedalPeak(int slot) const override { return m_dsp.pedalState().getPedalPeak(slot); }
    float getPedalGain(int slot) const override { return m_dsp.pedalState().getPedalGain(slot); }
    void setPedalGain(int slot, float gain) override { m_dsp.pedalState().setPedalGain(slot, gain); }
    float getInputGain() const override { return m_dsp.pedalState().getInputGain(); }
    void setInputGain(float gain) override { m_dsp.pedalState().setInputGain(gain); }
    float getOutputGain() const override { return m_dsp.pedalState().getOutputGain(); }
    void setOutputGain(float gain) override { m_dsp.pedalState().setOutputGain(gain); }

    // --- IBottomBarModel ---
    void setBarCount(int b) override { m_barCount = b; }
    int getBarCount() const override { return m_barCount; }
    void setSectionStart(int s) override { m_sectionStartBar = s; }
    int getSectionStart() const override { return m_sectionStartBar; }
    void setManualMode(bool m) override;
    bool isManualMode() const override { return m_manualMode; }

    // --- IConfigConsumer ---
    bool consumeCompiledResultIfAvailable() override;
    uint32_t getConfigRevision() const override { return m_configRevision.load(std::memory_order_acquire); }
    bool consumeUINotification() override;
    void triggerUINotification();
    const ConfigSyncData& getLastConfigSync() const override { return m_lastConfigSync; }
    const PedalAssetPayload* getCurrentConfig() const override { return m_currentConfig.load(std::memory_order_acquire); }
    bool isParamOverridden(int slot, int knob) const override { return m_dsp.isParamOverridden(slot, knob); }
    float getKnobDisplayValue(int slot, int knob, float val) const override { return m_dsp.getKnobDisplayValue(slot, knob, val); }
    void storeParameterValue(int slot, int knob, float v) override { m_dsp.storeParameterValue(slot, knob, v); }
    void resetPedalPeaks() override { m_dsp.pedalState().resetPedalPeaks(); }
    void setAutomationValue(float val) override { m_dsp.setAutomationValue(val); }
    float getKnobLinkStrength(int slot, int knob) const override { return m_dsp.pedalState().getKnobLinkStrength(slot, knob); }
    void drainReleaseQueue() override { m_releaseQueue.drain(); }
    void tryApplyDeferredConfig() override;
    std::vector<ParameterDescriptor> getCurrentParams() const;

    float getPlayHeadBpm() const override { return m_playHeadBpm.load(std::memory_order_acquire); }
    double getPlayHeadPpq() const override { return m_playHeadPpq.load(std::memory_order_acquire); }
    bool isPlayHeadPlaying() const override { return m_playHeadPlaying.load(std::memory_order_acquire); }
    void setPlayHeadPosition(float bpm, double ppq, bool playing);

    // --- Lifecycle ---
    void prepare(double sampleRate, int samplesPerBlock);
    void releaseResources();

    // --- UI helpers ---
    void notifyPenDown() { m_penDebouncer.penDown(); }
    void notifyPenUp() { m_penDebouncer.penUp(); }
    void submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data);

    // --- Undo ---
    void storeUndoData(std::vector<uint8_t> data) override { m_undoData = std::move(data); }
    const std::vector<uint8_t>& getUndoData() const override { return m_undoData; }

    // --- State serialization ---
    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    juce::MemoryBlock createPresetState();
    bool applyPresetState(const void* data, int sizeInBytes);

    // --- Audio thread config view ---
    ConfigAudioView getAudioView()
    {
        return { m_currentConfig, m_nextConfig, m_chainEffects, m_pendingEffects, m_releaseQueue, m_paramPtrs };
    }

private:
    void loadPedalConfiguration(const PedalAssetPayload* config);
    void prebuildEffects(const PedalAssetPayload* config, bool& deferred);
    void syncCompilerConfig();
    void restoreKnobValuesFromState(const StateSerializer::SerializedState& state);

    UnifiedPedalProcessor& m_dsp;
    CanvasMessageQueue m_messageQueue;
    CompilerThread m_compilerThread;
    PenDebouncer m_penDebouncer;
    std::array<uint8_t, TotalCells> m_gridData{};
    std::array<DspModuleType, PedalSlotCount> m_pedalSlots{};
    std::vector<uint8_t> m_manualRouting;
    int m_barCount = 1;
    int m_sectionStartBar = 0;
    bool m_manualMode = false;
    std::vector<uint8_t> m_undoData;
    std::atomic<float> m_playHeadBpm{120.0f};
    std::atomic<double> m_playHeadPpq{0.0};
    std::atomic<bool> m_playHeadPlaying{false};
    std::atomic<uint32_t> m_configRevision{0};
    std::atomic<bool> m_uiNeedsUpdate{false};
    ConfigSyncData m_lastConfigSync;

    // Config lifecycle (moved from UnifiedPedalProcessor)
    std::atomic<const PedalAssetPayload*> m_currentConfig{nullptr};
    std::atomic<const PedalAssetPayload*> m_nextConfig{nullptr};
    std::atomic<const PedalAssetPayload*> m_deferredConfig{nullptr};
    ReleaseQueue m_releaseQueue;
    std::array<std::unique_ptr<DspEffect>, PedalSlotCount> m_chainEffects;
    std::array<std::unique_ptr<DspEffect>, PedalSlotCount> m_pendingEffects;
    std::array<std::array<const float*, 4>, PedalSlotCount> m_paramPtrs{};
};
