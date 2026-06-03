#pragma once
#include <cstdint>
#include <memory>
#include <vector>

class DspState;
class UnifiedPedalProcessor;
struct PedalAssetPayload;

class HeadlessDspTest
{
public:
    HeadlessDspTest();
    ~HeadlessDspTest();

    bool initialize(double sampleRate, int maxSamplesPerBlock, int numChannels);
    void shutdown();

    void loadConfiguration(std::shared_ptr<PedalAssetPayload> config);
    void processAudio(float** input, float** output, int numSamples);
    void reset();

    double getSampleRate() const { return m_sampleRate; }
    int getMaxSamplesPerBlock() const { return m_maxSamplesPerBlock; }
    int getNumChannels() const { return m_numChannels; }

    // Validation helpers
    float getPeakOutputLevel() const;
    bool hasNaNOrInf() const;

private:
    double m_sampleRate = 44100.0;
    int m_maxSamplesPerBlock = 1024;
    int m_numChannels = 2;

    std::unique_ptr<DspState> m_dspState;
    std::unique_ptr<UnifiedPedalProcessor> m_dspProcessor;
};