#include "HeadlessDspTest.h"
#include "DspState.h"
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

    m_dspState = std::make_unique<DspState>();
    m_dspProcessor = std::make_unique<UnifiedPedalProcessor>();

    m_dspProcessor->prepareToPlay(sampleRate, maxSamplesPerBlock, numChannels);
    m_dspProcessor->reset();

    return true;
}

void HeadlessDspTest::shutdown()
{
    m_dspProcessor.reset();
    m_dspState.reset();
}

void HeadlessDspTest::loadConfiguration(std::shared_ptr<PedalAssetPayload> config)
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

float HeadlessDspTest::getPeakOutputLevel() const
{
    // This would need to track peak levels during processing
    // For now, return a placeholder
    return 0.0f;
}

bool HeadlessDspTest::hasNaNOrInf() const
{
    // Would check processed buffers for NaN/Inf
    return false;
}