#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cstdint>
#include "UnifiedPedalProcessor.h"
#include "CanvasMessageQueue.h"
#include "CompilerThread.h"
#include "PenDebouncer.h"
#include "PedalStructures.h"

class DrawdioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int SerializedSize = 4 + TotalCells + 6 + 6 + 1;

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

    void setPedalSlot(int slot, DspModuleType type);
    DspModuleType getPedalSlot(int slot) const;
    void setGridData(const std::array<uint8_t, GridSize * GridSize>& data);
    const std::array<uint8_t, GridSize * GridSize>& getGridData() const { return m_gridData; }

    void setManualRouting(const std::vector<uint8_t>& routing);
    const std::vector<uint8_t>& getManualRouting() const { return m_manualRouting; }

private:
    void syncCompilerConfig();

    UnifiedPedalProcessor m_dspProcessor;
    CanvasMessageQueue m_messageQueue;
    CompilerThread m_compilerThread;
    PenDebouncer m_penDebouncer;
    std::array<uint8_t, GridSize * GridSize> m_gridData;
    std::array<DspModuleType, 6> m_pedalSlots;
    std::vector<uint8_t> m_manualRouting;
    std::vector<float*> m_channelBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawdioProcessor)
};
