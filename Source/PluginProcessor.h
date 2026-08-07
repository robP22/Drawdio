#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "Core/CompiledPedalConfig.h"
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "Core/Contracts/ProcessorInterfaces.h"
#include "Core/Contracts/IConfigConsumer.h"
#include "UnifiedPedalProcessor.h"
#include "State/ConfigManager.h"
#include "State/ProcessorState.h"

class DrawdioProcessor : public juce::AudioProcessor,
                         public IPedalboardModel,
                         public IBottomBarModel,
                         public IConfigConsumer
{
public:
    DrawdioProcessor();
    ~DrawdioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    bool silenceInProducesSilenceOut() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- IPedalboardModel ---
    void setPedalSlot(int slot, DspModuleType type) override { m_config.setPedalSlot(slot, type); }
    DspModuleType getPedalSlot(int slot) const override { return m_config.getPedalSlot(slot); }
    void setGridData(const std::array<uint8_t, TotalCells>& data) { m_config.setGridData(data); }
    const std::array<uint8_t, TotalCells>& getGridData() const override { return m_config.getGridData(); }
    void setManualRouting(const std::vector<uint8_t>& routing) override { m_config.setManualRouting(routing); }
    const std::vector<uint8_t>& getManualRouting() const override { return m_config.getManualRouting(); }
    std::array<float, TotalKnobs> getKnobValues() const override { return m_config.getKnobValues(); }
    uint32_t getParamOverrideMask() const { return m_config.getParamOverrideMask(); }
    void setKnobParameter(int slot, int knob, float dragStartValue, float newValue) override { m_config.setKnobParameter(slot, knob, dragStartValue, newValue); }
    bool isKnobLinked(int slot, int knob) const override { return m_config.isKnobLinked(slot, knob); }
    void setKnobLink(int slot, int knob, bool linked) override { m_config.setKnobLink(slot, knob, linked); }
    float getPedalPeak(int slot) const override { return m_config.getPedalPeak(slot); }
    float getPedalGain(int slot) const override { return m_config.getPedalGain(slot); }
    void setPedalGain(int slot, float gain) override { m_config.setPedalGain(slot, gain); }
    float getInputGain() const override { return m_config.getInputGain(); }
    void setInputGain(float gain) override { m_config.setInputGain(gain); }
    float getOutputGain() const override { return m_config.getOutputGain(); }
    void setOutputGain(float gain) override { m_config.setOutputGain(gain); }

    // --- IBottomBarModel ---
    void setBarCount(int b) override { m_config.setBarCount(b); }
    int getBarCount() const override { return m_config.getBarCount(); }
    void setSectionStart(int s) override { m_config.setSectionStart(s); }
    int getSectionStart() const override { return m_config.getSectionStart(); }
    void setManualMode(bool m) override { m_config.setManualMode(m); }
    bool isManualMode() const override { return m_config.isManualMode(); }

    // --- IConfigConsumer ---
    bool consumeCompiledResultIfAvailable() override { return m_config.consumeCompiledResultIfAvailable(); }
    uint32_t getConfigRevision() const override { return m_config.getConfigRevision(); }
    bool consumeUINotification() override { return m_config.consumeUINotification(); }
    void triggerUINotification() { m_config.triggerUINotification(); }
    const ConfigSyncData& getLastConfigSync() const override { return m_config.getLastConfigSync(); }
    const PedalAssetPayload* getCurrentConfig() const override { return m_config.getCurrentConfig(); }
    bool isParamOverridden(int slot, int knob) const override { return m_config.isParamOverridden(slot, knob); }
    float getKnobDisplayValue(int slot, int knob, float val) const override { return m_config.getKnobDisplayValue(slot, knob, val); }
    void storeParameterValue(int slot, int knob, float v) override { m_config.storeParameterValue(slot, knob, v); }
    void resetPedalPeaks() override { m_config.resetPedalPeaks(); }
    void setAutomationValue(float val) override { m_config.setAutomationValue(val); }
    float getKnobLinkStrength(int slot, int knob) const override { return m_config.getKnobLinkStrength(slot, knob); }
    void drainReleaseQueue() override { m_config.drainReleaseQueue(); }
    void tryApplyDeferredConfig() override { m_config.tryApplyDeferredConfig(); }
    float getPlayHeadBpm() const override { return m_config.getPlayHeadBpm(); }
    double getPlayHeadPpq() const override { return m_config.getPlayHeadPpq(); }
    bool isPlayHeadPlaying() const override { return m_config.isPlayHeadPlaying(); }
    void storeUndoData(std::vector<uint8_t> data) override { m_config.storeUndoData(std::move(data)); }
    const std::vector<uint8_t>& getUndoData() const override { return m_config.getUndoData(); }

    // --- UI Facade ---
    void notifyPenDown() { m_config.notifyPenDown(); }
    void notifyPenUp() { m_config.notifyPenUp(); }
    void submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data) { m_config.submitCanvasSnapshot(data); }
    void scheduleReset() { m_dspProcessor.scheduleReset(); }
    void clearParamOffsets() { m_dspProcessor.clearParamOffsets(); }
    float getInputMeterLevel() const { return m_processorState.getInputMeterLevel(); }
    float getOutputMeterLevel() const { return m_processorState.getOutputMeterLevel(); }

private:
    bool allEffectsSilent() const;
    UnifiedPedalProcessor m_dspProcessor;
    ConfigManager m_config;
    ProcessorState m_processorState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawdioProcessor)
};
