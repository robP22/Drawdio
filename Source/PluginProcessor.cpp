#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StateSerializer.h"

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

    // Pre-allocate channel buffer for maximum channel count — no allocations on audio thread
    m_channelBuffer.assign(static_cast<size_t>(maxChannels), nullptr);

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

    if (auto* playHead = getPlayHead())
    {
        auto pos = playHead->getPosition();
        if (pos.hasValue())
        {
            m_playHeadBpm.store(static_cast<float>(pos->getBpm().orFallback(120.0)),
                                std::memory_order_relaxed);
            m_playHeadPpq.store(pos->getPpqPosition().orFallback(0.0), std::memory_order_relaxed);
            m_playHeadPlaying.store(pos->getIsPlaying(), std::memory_order_relaxed);
        }
    }

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto fastPeak = [&](int numCh) -> float {
        for (int c = 0; c < std::min(numCh, buffer.getNumChannels()); ++c)
        {
            const auto* d = buffer.getReadPointer(c);
            for (int s = 0; s < std::min(4, buffer.getNumSamples()); ++s)
                if (std::abs(d[s]) > 1e-6f)
                    return calculatePeak(buffer, numCh);
        }
        return 0.0f;
    };

    const auto inputPeak = fastPeak(totalNumInputChannels);

    for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        m_channelBuffer[static_cast<size_t>(ch)] = buffer.getWritePointer(ch);

    m_dspProcessor.processAudioBlock(m_channelBuffer.data(),
                                     totalNumOutputChannels,
                                     buffer.getNumSamples());

    publishMeterLevels(inputPeak, fastPeak(totalNumOutputChannels));
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
        DspModuleType oldType = m_pedalSlots[static_cast<size_t>(slot)];
        m_pedalSlots[static_cast<size_t>(slot)] = type;

        if (type == DspModuleType::BYPASS)
        {
            std::vector<uint8_t> filtered;
            for (auto s : m_manualRouting)
                if (s != static_cast<uint8_t>(slot))
                    filtered.push_back(s);
            m_manualRouting = filtered;
        }
        else if (oldType == DspModuleType::BYPASS && !m_manualRouting.empty())
        {
            if (std::find(m_manualRouting.begin(), m_manualRouting.end(),
                          static_cast<uint8_t>(slot)) == m_manualRouting.end())
                m_manualRouting.push_back(static_cast<uint8_t>(slot));
        }

        m_dspProcessor.invalidateParamCacheForSlot(slot);
        syncCompilerConfig();
        triggerUINotification();
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

void DrawdioProcessor::setManualMode(bool m)
{
    m_manualMode = m;
    if (!m)
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

    auto* payloadPtr = m_compilerThread.getCompiledPayloadPtr();
    if (!payloadPtr)
        return false;

    m_lastConfigSync.parameters = payloadPtr->parameters;
    m_lastConfigSync.routingSlotOrder = payloadPtr->routingSlotOrder;
    m_dspProcessor.loadPedalConfiguration(payloadPtr);
    m_configRevision.fetch_add(1, std::memory_order_acq_rel);
    triggerUINotification();
    return true;
}

bool DrawdioProcessor::consumeUINotification()
{
    bool expected = true;
    return m_uiNeedsUpdate.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
}

void DrawdioProcessor::triggerUINotification()
{
    m_uiNeedsUpdate.store(true, std::memory_order_release);
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
    auto snap = m_dspProcessor.getSnapshot();
    auto mask = m_dspProcessor.getParamOverrideMask();
    auto state = StateSerializer::createState(m_gridData, m_pedalSlots, m_manualRouting, snap.values, mask,
                                              static_cast<uint8_t>(m_barCount),
                                              static_cast<uint8_t>(m_sectionStartBar),
                                              static_cast<uint8_t>(m_manualMode ? 1 : 0));
    StateSerializer::serialize(state, destData);
}

void DrawdioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    StateSerializer::SerializedState state;
    if (!StateSerializer::deserialize(static_cast<const uint8_t*>(data),
                                      static_cast<size_t>(sizeInBytes), state))
        return;

    m_gridData = state.gridData;
    m_pedalSlots = state.pedalSlots;
    m_manualRouting = state.manualRouting;
    restoreKnobValuesFromState(state);
    m_barCount = state.barCount;
    m_sectionStartBar = state.sectionStartBar;
    m_manualMode = (state.manualMode != 0);
    m_dspProcessor.scheduleReset();
    syncCompilerConfig();
}

juce::MemoryBlock DrawdioProcessor::createPresetState()
{
    auto snap = m_dspProcessor.getSnapshot();
    auto mask = m_dspProcessor.getParamOverrideMask();
    auto state = StateSerializer::createState(m_gridData, m_pedalSlots, m_manualRouting, snap.values, mask,
                                              static_cast<uint8_t>(m_barCount),
                                              static_cast<uint8_t>(m_sectionStartBar),
                                              static_cast<uint8_t>(m_manualMode ? 1 : 0));
    juce::MemoryBlock result;
    StateSerializer::serialize(state, result);
    return result;
}

bool DrawdioProcessor::applyPresetState(const void* data, int sizeInBytes)
{
    StateSerializer::SerializedState state;
    if (!StateSerializer::deserialize(static_cast<const uint8_t*>(data),
                                     static_cast<size_t>(sizeInBytes), state))
        return false;

    m_gridData = state.gridData;
    m_pedalSlots = state.pedalSlots;
    m_manualRouting = state.manualRouting;
    restoreKnobValuesFromState(state);
    m_dspProcessor.scheduleReset();
    syncCompilerConfig();
    return true;
}

void DrawdioProcessor::restoreKnobValuesFromState(const StateSerializer::SerializedState& state)
{
    for (int s = 0; s < PedalSlotCount; ++s)
        for (int k = 0; k < 4; ++k)
        {
            size_t idx = static_cast<size_t>(s * 4 + k);
            if (state.overrideMask & (1u << idx))
                m_dspProcessor.updateParameter(s, k, state.knobValues[idx]);
            else
                m_dspProcessor.storeParameterValue(s, k, state.knobValues[idx]);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrawdioProcessor();
}
