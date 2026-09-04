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
#include "State/AutomationEnvelope.h"

struct ConfigAudioView
{
    std::atomic<const PedalAssetPayload*>& currentConfig;
    std::atomic<const PedalAssetPayload*>& nextConfig;
    ReleaseQueue& releaseQueue;
};

class ConfigManager : public juce::ChangeBroadcaster,
                      public IPedalboardModel,
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
    std::array<float, TotalKnobs> getKnobValues() const override { return m_dsp.getSnapshot().values; }
    uint32_t getParamOverrideMask() const { return m_dsp.getParamOverrideMask(); }
    void setKnobParameter(int slot, int knob, float dragStartValue, float newValue) override { m_dsp.setKnobParameter(slot, knob, dragStartValue, newValue); }
    bool isKnobLinked(int slot, int knob) const override { return m_dsp.pedalState().isKnobLinked(slot, knob); }
    void setKnobLink(int slot, int knob, bool linked) override
    {
        m_dsp.pedalState().setKnobLink(slot, knob, linked, 1.0f);
        if (linked)
            m_dsp.pedalState().setKnobLinkRange(slot, knob, 0.0f, 1.0f);
        triggerUINotification();
    }
    void setKnobLinkRange(int slot, int knob, float rangeMin, float rangeMax) override
    {
        m_dsp.pedalState().setKnobLinkRange(slot, knob, rangeMin, rangeMax);
        triggerUINotification();
    }
    float getKnobLinkRangeMin(int slot, int knob) const override { return m_dsp.pedalState().getKnobLinkRangeMin(slot, knob); }
    float getKnobLinkRangeMax(int slot, int knob) const override { return m_dsp.pedalState().getKnobLinkRangeMax(slot, knob); }
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

    void setManualEnvelopeSlice(int slice, float value);
    float getManualEnvelopeSlice(int slice) const;
    AutomationEnvelope getManualEnvelope() const override { return m_manualEnvelope; }
    bool hasManualEnvelope() const override { return m_hasManualEnvelope; }

    // --- IConfigConsumer ---
    bool consumeCompiledResultIfAvailable() override;
    uint32_t getConfigRevision() const override { return m_configRevision.load(std::memory_order_acquire); }
    bool consumeUINotification() override;
    void triggerUINotification();
    const ConfigSyncData& getLastConfigSync() const override { return m_lastConfigSync; }
    const PedalAssetPayload* getCurrentConfig() const override { return m_currentConfig.load(std::memory_order_acquire); }
    bool hasPendingConfig() const { return m_nextConfig.load(std::memory_order_acquire) != nullptr; }
    bool isParamOverridden(int slot, int knob) const override { return m_dsp.isParamOverridden(slot, knob); }
    float getKnobDisplayValue(int slot, int knob, float val) const override { return m_dsp.getKnobDisplayValue(slot, knob, val); }
    void storeParameterValue(int slot, int knob, float v) override { m_dsp.storeParameterValue(slot, knob, v); }
    void resetPedalPeaks() override { m_dsp.pedalState().resetPedalPeaks(); }
    void setAutomationValue(float val) override { m_dsp.setAutomationValue(val); }
    float getKnobLinkStrength(int slot, int knob) const override { return m_dsp.pedalState().getKnobLinkStrength(slot, knob); }
    void drainReleaseQueue() override { m_releaseQueue.drain(); }
    uint32_t getReleaseQueueDroppedCount() const override { return m_releaseQueue.droppedCount(); }
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
    void submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data,
                              const DirtyRowMask& dirtyRows);
    void resetParamDefaults();

    // --- Undo ---
    void storeUndoData(std::vector<uint8_t> data) override { m_undoData = std::move(data); }
    const std::vector<uint8_t>& getUndoData() const override { return m_undoData; }

    // --- State serialization ---
    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    void getPresetInformation(juce::MemoryBlock& destData) const;
    bool setPresetInformation(const void* data, int sizeInBytes);

    EditorSessionState getEditorSessionState() const { return m_sessionState; }
    void setEditorSessionState(const EditorSessionState& state)
    {
        if (m_sessionState.selectedColour == state.selectedColour
            && m_sessionState.selectedTool == state.selectedTool
            && m_sessionState.selectedPedal == state.selectedPedal
            && m_sessionState.brushSizeIndex == state.brushSizeIndex
            && m_sessionState.linkRangeEditEnabled == state.linkRangeEditEnabled)
            return;
        m_sessionState = state;
        m_sessionRevision.fetch_add(1, std::memory_order_acq_rel);
        triggerUINotification();
    }
    uint32_t getSessionRevision() const { return m_sessionRevision.load(std::memory_order_acquire); }
    void addEditorStateListener(juce::ChangeListener* listener) { addChangeListener(listener); }
    void removeEditorStateListener(juce::ChangeListener* listener) { removeChangeListener(listener); }

    // --- Audio thread config view ---
    ConfigAudioView getAudioView()
    {
        return { m_currentConfig, m_nextConfig, m_releaseQueue };
    }

private:
    void loadPedalConfiguration(PedalAssetPayload* config);
    void prebuildEffects(PedalAssetPayload* config, bool& deferred);
    void rePrepareEffects();
    void syncCompilerConfig();
    void retryPendingCompile();
    PresetState capturePresetState() const;
    void applyPresetState(const PresetState& state);
    void restoreKnobValuesFromState(const PresetState& state);
    void syncLastConfig(const PedalAssetPayload* config);
    void publishCompiledParameters(const PedalAssetPayload* config);
    void updateCompiledParameterBank(const PedalAssetPayload* config);
    void seedCacheFromCurrentConfig();

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
    AutomationEnvelope m_manualEnvelope;
    bool m_hasManualEnvelope = false;
    EditorSessionState m_sessionState;
    std::vector<uint8_t> m_undoData;
    std::atomic<float> m_playHeadBpm{120.0f};
    std::atomic<double> m_playHeadPpq{0.0};
    std::atomic<bool> m_playHeadPlaying{false};
    std::atomic<uint32_t> m_configRevision{0};
    std::atomic<uint32_t> m_sessionRevision{0};
    uint32_t m_canvasRevision = 0;
    std::atomic<bool> m_uiNeedsUpdate{false};
    bool m_compileRetryPending = false;
    ConfigSyncData m_lastConfigSync;

    // Config lifecycle (moved from UnifiedPedalProcessor)
    std::atomic<const PedalAssetPayload*> m_currentConfig{nullptr};
    std::atomic<const PedalAssetPayload*> m_nextConfig{nullptr};
    std::atomic<PedalAssetPayload*> m_deferredConfig{nullptr};
    ReleaseQueue m_releaseQueue;
    double m_sampleRate = 44100.0;
    int m_preparedBlockSize = 0;
    int m_preparedChannels = 0;
};
