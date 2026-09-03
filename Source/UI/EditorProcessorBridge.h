#pragma once

#include <JuceHeader.h>
#include <vector>

#include "UI/EditorState.h"
#include "Core/Contracts/IConfigConsumer.h"

class DrawdioProcessor;

class EditorProcessorBridge : private juce::ChangeListener
{
public:
    explicit EditorProcessorBridge(DrawdioProcessor& processor);
    ~EditorProcessorBridge() override;

    void detach();

    std::function<void()> onStateChanged;

    EditorUiSnapshot getUiSnapshot() const;

    void setPedalSlot(int slot, DspModuleType type);
    void setManualRouting(const std::vector<uint8_t>& routing);
    void setKnobParameter(int slot, int knob, float dragStartValue, float newValue);
    void setKnobLink(int slot, int knob, bool linked);
    void setKnobLinkRange(int slot, int knob, float rangeMin, float rangeMax);
    float getKnobLinkRangeMin(int slot, int knob) const;
    float getKnobLinkRangeMax(int slot, int knob) const;
    void setPedalGain(int slot, float gain);
    void setInputGain(float gain);
    void setOutputGain(float gain);
    void setBarCount(int bars);
    void setSectionStart(int section);
    void setManualMode(bool manual);
    void setAutomationValue(float value);
    void notifyPenDown();
    void notifyPenUp();
    void submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data);
    void submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data,
                              const DirtyRowMask& dirtyRows);
    void scheduleReset();
    void clearParameterOffsets();
    void resetParameterDefaults();
    void storeUndoData(std::vector<uint8_t> data);
    const std::vector<uint8_t>& getUndoData() const;
    void storeParameterValue(int slot, int knob, float value);
    float getPlayHeadBpm() const;
    double getPlayHeadPpq() const;
    bool isPlayHeadPlaying() const;

    void getProjectInformation(juce::MemoryBlock& data) const;
    void getPresetInformation(juce::MemoryBlock& data) const;
    bool setPresetInformation(const void* data, int sizeInBytes);
    EditorSessionState getEditorSessionState() const;
    void setEditorSessionState(const EditorSessionState& state);

    void drainReleaseQueue();
    uint32_t getReleaseQueueDroppedCount() const;
    void tryApplyDeferredConfig();
    bool consumeUINotification();
    bool consumeCompiledResultIfAvailable();
    uint32_t getConfigRevision() const;
    const ConfigSyncData& getLastConfigSync() const;
    const std::vector<uint8_t>& getManualRouting() const;
    bool isManualMode() const;
    bool isParamOverridden(int slot, int knob) const;
    float getKnobDisplayValue(int slot, int knob, float compiled) const;
    std::array<float, TotalKnobs> getKnobValues() const;
    bool isKnobLinked(int slot, int knob) const;
    float getKnobLinkStrength(int slot, int knob) const;
    DspModuleType getPedalSlot(int slot) const;
    float getPedalPeak(int slot) const;
    const std::array<uint8_t, TotalCells>& getGridData() const;
    int getBarCount() const;
    int getSectionStart() const;
    void resetPedalPeaks();

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    DrawdioProcessor& m_processor;
    bool m_attached = true;
};
