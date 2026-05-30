#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

DrawdioProcessor::DrawdioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    m_gridData.fill(0);
    m_pedalSlots.fill(DspModuleType::BYPASS);
}

DrawdioProcessor::~DrawdioProcessor() {}

void DrawdioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    int maxChannels = getTotalNumOutputChannels();
    m_dspProcessor.prepareToPlay(sampleRate, samplesPerBlock, maxChannels);
    m_channelBuffer.resize(maxChannels);

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
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (m_compilerThread.hasCompiledResult())
    {
        auto payloadPtr = m_compilerThread.getCompiledPayloadPtr();
        if (payloadPtr)
            m_dspProcessor.loadPedalConfiguration(std::move(payloadPtr));
    }

    if (static_cast<int>(m_channelBuffer.size()) < totalNumOutputChannels)
        m_channelBuffer.resize(totalNumOutputChannels);
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        m_channelBuffer[static_cast<size_t>(ch)] = buffer.getWritePointer(ch);

    m_dspProcessor.processAudioBlock(m_channelBuffer.data(), totalNumOutputChannels, buffer.getNumSamples());
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
const juce::String DrawdioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void DrawdioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void DrawdioProcessor::setPedalSlot(int slot, DspModuleType type)
{
    if (slot >= 0 && slot < 6)
    {
        m_pedalSlots[slot] = type;
        syncCompilerConfig();
    }
}

DspModuleType DrawdioProcessor::getPedalSlot(int slot) const
{
    if (slot >= 0 && slot < 6)
        return m_pedalSlots[slot];
    return DspModuleType::BYPASS;
}

void DrawdioProcessor::setGridData(const std::array<uint8_t, GridSize * GridSize>& data)
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

void DrawdioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    std::vector<uint8_t> blob(SerializedSize, 0);

    blob[0] = 0x44; // 'D'
    blob[1] = 0x52; // 'R'
    blob[2] = 0x44; // 'D'
    blob[3] = 0x01; // version

    std::memcpy(blob.data() + 4, m_gridData.data(), GridSize * GridSize);

    int layoutOffset = 4 + GridSize * GridSize;
    for (int i = 0; i < 6; ++i)
        blob[layoutOffset + i] = static_cast<uint8_t>(m_pedalSlots[i]);

    int routingOffset = layoutOffset + 6;
    for (int i = 0; i < 6; ++i)
    {
        if (i < static_cast<int>(m_manualRouting.size()))
            blob[routingOffset + i] = m_manualRouting[i];
        else
            blob[routingOffset + i] = 0xFF; // Sentinel for empty
    }

    int flagOffset = routingOffset + 6;
    blob[flagOffset] = 0;

    destData.setSize(SerializedSize);
    destData.copyFrom(blob.data(), 0, SerializedSize);
}

void DrawdioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (sizeInBytes < SerializedSize) return;

    const uint8_t* blob = static_cast<const uint8_t*>(data);

    if (blob[0] != 0x44 || blob[1] != 0x52 || blob[2] != 0x44)
        return;

    std::memcpy(m_gridData.data(), blob + 4, GridSize * GridSize);
    for (auto& val : m_gridData)
        if (val > 4) val = 0;

    int layoutOffset = 4 + GridSize * GridSize;
    constexpr auto maxPedalType = static_cast<uint8_t>(DspModuleType::GRANULAR_DELAY);
    for (int i = 0; i < 6; ++i)
    {
        uint8_t raw = blob[layoutOffset + i];
        if (raw > maxPedalType) raw = 0;
        m_pedalSlots[i] = static_cast<DspModuleType>(raw);
    }

    int routingOffset = layoutOffset + 6;
    m_manualRouting.clear();
    for (int i = 0; i < 6; ++i)
    {
        uint8_t slot = blob[routingOffset + i];
        if (slot < 6)
            m_manualRouting.push_back(slot);
    }

    m_dspProcessor.reset();
    syncCompilerConfig();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrawdioProcessor();
}
