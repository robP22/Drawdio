#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace
{
float calculatePeak(const juce::AudioBuffer<float>& buffer, int channelCount)
{
    float peak = 0.0f;
    const auto channels = juce::jlimit(0, buffer.getNumChannels(), channelCount);

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* samples = buffer.getReadPointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            peak = std::max(peak, std::abs(samples[s]));
    }

    return juce::jlimit(0.0f, 1.0f, peak);
}
}

DrawdioProcessor::DrawdioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    m_gridData.fill(0);
    m_pedalSlots.fill(DspModuleType::BYPASS);
}

DrawdioProcessor::~DrawdioProcessor() = default;

void DrawdioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const int maxChannels = getTotalNumOutputChannels();
    m_dspProcessor.prepareToPlay(sampleRate, samplesPerBlock, maxChannels);
    m_channelBuffer.resize(static_cast<size_t>(maxChannels));

    std::vector<DspModuleType> slots(m_pedalSlots.begin(), m_pedalSlots.end());
    m_compilerThread.setPedalSlots(slots);
    m_compilerThread.start(m_messageQueue, m_penDebouncer);
}

void DrawdioProcessor::releaseResources()
{
    m_compilerThread.stop();
}

void DrawdioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    const auto inputPeak = calculatePeak(buffer, totalNumInputChannels);

    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    consumeCompiledResultIfAvailable();

    if (static_cast<int>(m_channelBuffer.size()) < totalNumOutputChannels)
        m_channelBuffer.resize(static_cast<size_t>(totalNumOutputChannels));

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        m_channelBuffer[static_cast<size_t>(ch)] = buffer.getWritePointer(ch);

    m_dspProcessor.processAudioBlock(m_channelBuffer.data(),
                                     totalNumOutputChannels,
                                     buffer.getNumSamples());

    publishMeterLevels(inputPeak, calculatePeak(buffer, totalNumOutputChannels));
}

juce::AudioProcessorEditor* DrawdioProcessor::createEditor()
{
    return new DrawdioProcessorEditor(*this);
}

bool DrawdioProcessor::hasEditor() const { return true; }

const juce::String DrawdioProcessor::getName() const { return JucePlugin_Name; }

bool DrawdioProcessor::acceptsMidi() const { return false; }
bool DrawdioProcessor::producesMidi() const { return false; }
bool DrawdioProcessor::isMidiEffect() const { return false; }
double DrawdioProcessor::getTailLengthSeconds() const { return 3.0; }

int DrawdioProcessor::getNumPrograms() { return 1; }
int DrawdioProcessor::getCurrentProgram() { return 0; }
void DrawdioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String DrawdioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}
void DrawdioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void DrawdioProcessor::setPedalSlot(int slot, DspModuleType type)
{
    if (slot >= 0 && slot < static_cast<int>(m_pedalSlots.size()))
    {
        m_pedalSlots[static_cast<size_t>(slot)] = type;
        syncCompilerConfig();
    }
}

DspModuleType DrawdioProcessor::getPedalSlot(int slot) const
{
    if (slot >= 0 && slot < static_cast<int>(m_pedalSlots.size()))
        return m_pedalSlots[static_cast<size_t>(slot)];

    return DspModuleType::BYPASS;
}

void DrawdioProcessor::setGridData(const std::array<uint8_t, TotalCells>& data)
{
    m_gridData = data;
}

void DrawdioProcessor::setManualRouting(const std::vector<uint8_t>& routing)
{
    m_manualRouting = routing;
    syncCompilerConfig();
}

void DrawdioProcessor::syncCompilerConfig()
{
    std::vector<DspModuleType> slots(m_pedalSlots.begin(), m_pedalSlots.end());
    m_compilerThread.setPedalSlots(slots);
    m_compilerThread.setManualRouting(m_manualRouting);
    m_compilerThread.setExistingParameters(m_dspProcessor.getCurrentParams());
    m_messageQueue.pushSnapshot(m_gridData.data());
    m_compilerThread.notify();
}

bool DrawdioProcessor::consumeCompiledResultIfAvailable()
{
    if (!m_compilerThread.hasCompiledResult())
        return false;

    auto payloadPtr = m_compilerThread.getCompiledPayloadPtr();
    if (!payloadPtr)
        return false;

    m_dspProcessor.loadPedalConfiguration(std::move(payloadPtr));
    m_configRevision.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

void DrawdioProcessor::publishMeterLevels(float inputPeak, float outputPeak)
{
    const auto decay = 0.82f;
    const auto previousInput = m_inputMeterLevel.load(std::memory_order_relaxed);
    const auto previousOutput = m_outputMeterLevel.load(std::memory_order_relaxed);

    m_inputMeterLevel.store(std::max(inputPeak, previousInput * decay), std::memory_order_relaxed);
    m_outputMeterLevel.store(std::max(outputPeak, previousOutput * decay), std::memory_order_relaxed);
}

void DrawdioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    std::vector<uint8_t> blob(SerializedSize, 0);

    blob[0] = 0x44; // 'D'
    blob[1] = 0x52; // 'R'
    blob[2] = 0x44; // 'D'
    blob[3] = 0x01; // version

    std::memcpy(blob.data() + 4, m_gridData.data(), TotalCells);

    const int layoutOffset = 4 + TotalCells;
    for (int i = 0; i < static_cast<int>(m_pedalSlots.size()); ++i)
        blob[layoutOffset + i] = static_cast<uint8_t>(m_pedalSlots[static_cast<size_t>(i)]);

    const int routingOffset = layoutOffset + PedalSlotCount;
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        if (i < static_cast<int>(m_manualRouting.size()))
            blob[routingOffset + i] = m_manualRouting[static_cast<size_t>(i)];
        else
            blob[routingOffset + i] = 0xFF;
    }

    const int flagOffset = routingOffset + PedalSlotCount;
    blob[flagOffset] = 0;

    destData.setSize(SerializedSize);
    destData.copyFrom(blob.data(), 0, SerializedSize);
}

void DrawdioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (sizeInBytes < SerializedSize)
        return;

    const auto* blob = static_cast<const uint8_t*>(data);

    if (blob[0] != 0x44 || blob[1] != 0x52 || blob[2] != 0x44)
        return;

    std::memcpy(m_gridData.data(), blob + 4, TotalCells);
    for (auto& val : m_gridData)
        if (val > 4)
            val = 0;

    const int layoutOffset = 4 + TotalCells;
    constexpr auto maxPedalType = static_cast<uint8_t>(DspModuleType::GRANULAR_DELAY);
    for (int i = 0; i < static_cast<int>(m_pedalSlots.size()); ++i)
    {
        uint8_t raw = blob[layoutOffset + i];
        if (raw > maxPedalType)
            raw = 0;

        m_pedalSlots[static_cast<size_t>(i)] = static_cast<DspModuleType>(raw);
    }

    const int routingOffset = layoutOffset + PedalSlotCount;
    m_manualRouting.clear();
    for (int i = 0; i < PedalSlotCount; ++i)
    {
        uint8_t slot = blob[routingOffset + i];
        if (slot < PedalSlotCount)
            m_manualRouting.push_back(slot);
    }

    m_dspProcessor.reset();
    syncCompilerConfig();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrawdioProcessor();
}
