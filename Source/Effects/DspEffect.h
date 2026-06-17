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
    virtual void processSample(float** buffer, int numChannels, int sampleNum, float driveParam) = 0;

    virtual int mixKnobIndex() const { return -1; }

    virtual void processBlock(float** buffer, int numChannels, int numSamples, const float* params)
    {
        for (int s = 0; s < numSamples; ++s)
            processSample(buffer, numChannels, s, params[3]);
    }

protected:
    double m_sampleRate = 44100.0;
    int m_numChannels = 2;
};
