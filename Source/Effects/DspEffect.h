#pragma once

class DspEffect
{
public:
    DspEffect(int mixKnobIndex = -1) : m_mixKnobIndex(mixKnobIndex) {}
    virtual ~DspEffect() = default;

    virtual void prepare(double sampleRate, int numChannels)
    {
        m_sampleRate = sampleRate;
        m_numChannels = numChannels;
    }

    virtual void reset() = 0;
    virtual void processSample(float** buffer, int numChannels, int sampleNum, float driveParam) = 0;

    // Called from the audio thread once per block. Effects that need musical
    // timing can override this without forcing transport arguments through
    // every processBlock implementation.
    virtual void setTransport(float bpm, double ppqPosition, bool isPlaying)
    {
        (void)bpm;
        (void)ppqPosition;
        (void)isPlaying;
    }

    int mixKnobIndex() const { return m_mixKnobIndex; }

    virtual bool hasActiveTail() const { return false; }
    virtual bool requiresContinuousProcessing() const { return true; }
    virtual double getTailLength() const { return 0.0; }

    virtual void processBlock(float** buffer, int numChannels, int numSamples, const float* params) = 0;

protected:
    const int m_mixKnobIndex;
    double m_sampleRate = 44100.0;
    int m_numChannels = 2;
};
