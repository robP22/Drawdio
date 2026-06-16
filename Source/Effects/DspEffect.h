#pragma once

class DspEffect
{
public:
    virtual ~DspEffect() = default;

    virtual void prepare(double sampleRate, int numChannels)
    {
        m_sampleRate = sampleRate;
        m_numChannels = numChannels;
    }

    virtual void reset() = 0;
    virtual void processSample(float** buffer, int numChannels, int sampleNum, float effectParam) = 0;
    virtual void setVolumeParam(float) {}

    virtual void processBlock(float** buffer, int numChannels, int numSamples, float effectParam)
    {
        for (int s = 0; s < numSamples; ++s)
            processSample(buffer, numChannels, s, effectParam);
    }

protected:
    double m_sampleRate = 44100.0;
    int m_numChannels = 2;
};
