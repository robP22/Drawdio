#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>


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
    , m_config(m_dspProcessor)
{
}

DrawdioProcessor::~DrawdioProcessor() = default;

void DrawdioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const int maxChannels = getTotalNumOutputChannels();
    m_dspProcessor.prepareToPlay(sampleRate, samplesPerBlock, maxChannels);

    m_processorState.prepare(maxChannels, samplesPerBlock);
    m_config.prepare(sampleRate, samplesPerBlock);
}

void DrawdioProcessor::releaseResources()
{
    m_config.releaseResources();
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
            m_config.setPlayHeadPosition(static_cast<float>(pos->getBpm().orFallback(120.0)),
                                          pos->getPpqPosition().orFallback(0.0),
                                          pos->getIsPlaying());
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

    auto& channelBuffer = m_processorState.getChannelBuffer();
    int bufCh = std::min(totalNumOutputChannels, static_cast<int>(channelBuffer.size()));
    for (int ch = 0; ch < bufCh; ++ch)
        channelBuffer[static_cast<size_t>(ch)] = buffer.getWritePointer(ch);

    auto cfgView = m_config.getAudioView();
    m_dspProcessor.processAudioBlock(channelBuffer.data(),
                                     bufCh,
                                     buffer.getNumSamples(),
                                     cfgView);

    m_processorState.publishMeterLevels(inputPeak, fastPeak(totalNumOutputChannels));
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

double DrawdioProcessor::getTailLengthSeconds() const
{
    if (const auto* config = m_config.getCurrentConfig())
    {
        double maxTail = 0.0;
        for (const auto& effect : config->effects)
            if (effect)
                maxTail = std::max(maxTail, effect->getTailLength());
        return maxTail;
    }
    return 5.0;
}

bool DrawdioProcessor::silenceInProducesSilenceOut() const
{
    if (const auto* config = m_config.getCurrentConfig())
        for (const auto& type : config->activeRoutingChain)
            if (type == DspModuleType::BITCRUSHER)
                return false;
    return true;
}

int DrawdioProcessor::getNumPrograms() { return 0; }
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

void DrawdioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    m_config.getStateInformation(destData);
}

void DrawdioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    m_config.setStateInformation(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrawdioProcessor();
}
