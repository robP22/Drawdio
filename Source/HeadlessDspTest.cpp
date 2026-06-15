#include "HeadlessDspTest.h"
#include "UnifiedPedalProcessor.h"
#include <cmath>
#include <algorithm>

HeadlessDspTest::HeadlessDspTest()
{
}

HeadlessDspTest::~HeadlessDspTest()
{
    shutdown();
}

bool HeadlessDspTest::initialize(double sampleRate, int maxSamplesPerBlock, int numChannels)
{
    m_sampleRate = sampleRate;
    m_maxSamplesPerBlock = maxSamplesPerBlock;
    m_numChannels = numChannels;

    m_dspProcessor = std::make_unique<UnifiedPedalProcessor>();

    m_dspProcessor->prepareToPlay(sampleRate, maxSamplesPerBlock, numChannels);
    m_dspProcessor->reset();

    return true;
}

void HeadlessDspTest::shutdown()
{
    m_dspProcessor.reset();
}

void HeadlessDspTest::loadConfiguration(const PedalAssetPayload* config)
{
    if (m_dspProcessor && config)
    {
        m_dspProcessor->loadPedalConfiguration(config);
    }
}

void HeadlessDspTest::processAudio(float** input, float** output, int numSamples)
{
    if (!m_dspProcessor || !input || !output)
        return;

    m_dspProcessor->processAudioBlock(input, m_numChannels, numSamples);

    // Copy output
    for (int ch = 0; ch < m_numChannels; ++ch)
    {
        if (input[ch] != output[ch])
        {
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = input[ch][i];
        }
    }
}

void HeadlessDspTest::reset()
{
    if (m_dspProcessor)
        m_dspProcessor->reset();
}

