#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

class ProcessorState
{
public:
    void prepare(int maxChannels, int samplesPerBlock)
    {
        juce::ignoreUnused(samplesPerBlock);
        m_channelBuffer.assign(static_cast<size_t>(maxChannels), nullptr);
    }

    std::vector<float*>& getChannelBuffer() { return m_channelBuffer; }

    void publishMeterLevels(float inputPeak, float outputPeak)
    {
        const auto decay = 0.82f;
        const auto prevInput = m_inputMeterLevel.load(std::memory_order_relaxed);
        const auto prevOutput = m_outputMeterLevel.load(std::memory_order_relaxed);

        m_inputMeterLevel.store(std::max(inputPeak, prevInput * decay), std::memory_order_relaxed);
        m_outputMeterLevel.store(std::max(outputPeak, prevOutput * decay), std::memory_order_relaxed);
    }

    float getInputMeterLevel() const { return m_inputMeterLevel.load(std::memory_order_relaxed); }
    float getOutputMeterLevel() const { return m_outputMeterLevel.load(std::memory_order_relaxed); }

private:
    std::vector<float*> m_channelBuffer;
    std::atomic<float> m_inputMeterLevel{0.0f};
    std::atomic<float> m_outputMeterLevel{0.0f};
};
