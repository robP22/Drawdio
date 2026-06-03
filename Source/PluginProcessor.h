#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "CanvasMessageQueue.h"
#include "CompilerThread.h"
#include "PedalStructures.h"
#include "PenDebouncer.h"
#include "UnifiedPedalProcessor.h"

class DrawdioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int SerializedSize = 4 + TotalCells + PedalSlotCount + PedalSlotCount + 1;

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

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    UnifiedPedalProcessor& getDSPProcessor() { return m_dspProcessor; }
    CanvasMessageQueue& getMessageQueue() { return m_messageQueue; }
    CompilerThread& getCompilerThread() { return m_compilerThread; }
    PenDebouncer& getPenDebouncer() { return m_penDebouncer; }
    bool consumeCompiledResultIfAvailable();
    uint32_t getConfigRevision() const { return m_configRevision.load(std::memory_order_acquire); }

    void setPedalSlot(int slot, DspModuleType type);
    DspModuleType getPedalSlot(int slot) const;
    void setGridData(const std::array<uint8_t, TotalCells>& data);
    const std::array<uint8_t, TotalCells>& getGridData() const { return m_gridData; }

    void setManualRouting(const std::vector<uint8_t>& routing);

    float getInputMeterLevel() const { return m_inputMeterLevel.load(std::memory_order_relaxed); }
    float getOutputMeterLevel() const { return m_outputMeterLevel.load(std::memory_order_relaxed); }

private:
    void syncCompilerConfig();
    void publishMeterLevels(float inputPeak, float outputPeak);

    UnifiedPedalProcessor m_dspProcessor;
    CanvasMessageQueue m_messageQueue;
    CompilerThread m_compilerThread;
    PenDebouncer m_penDebouncer;
    std::array<uint8_t, TotalCells> m_gridData;
    std::array<DspModuleType, PedalSlotCount> m_pedalSlots;
    std::vector<uint8_t> m_manualRouting;
    std::vector<float*> m_channelBuffer;

    std::atomic<float> m_inputMeterLevel { 0.0f };
    std::atomic<float> m_outputMeterLevel { 0.0f };
    std::atomic<uint32_t> m_configRevision { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawdioProcessor)
};
