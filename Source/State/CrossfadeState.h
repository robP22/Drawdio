#pragma once
#include <atomic>
#include <cmath>
#include <vector>

class CrossfadeState
{
public:
    static constexpr float kCrossfadeMs = 20.0f;

    void prepare(double sampleRate, int maxSamples, size_t maxChannels)
    {
        int cfSamps = std::max(1, static_cast<int>(kCrossfadeMs * sampleRate / 1000.0));
        m_crossfadeSamples.store(cfSamps, std::memory_order_release);

        m_crossfadeTempBuf.resize(maxChannels);
        for (auto& buf : m_crossfadeTempBuf)
            buf.assign(static_cast<size_t>(maxSamples), 0.0f);

        m_crossfadeOldOut.resize(maxChannels);
        for (auto& buf : m_crossfadeOldOut)
            buf.assign(static_cast<size_t>(maxSamples), 0.0f);

        m_crossfadeCounter = 0;
        m_pendingCrossfadeReset = false;
    }

    void reset() { m_crossfadeCounter = 0; }
    void requestReset() { m_pendingCrossfadeReset.store(true, std::memory_order_release); }
    bool consumeResetRequest() { return m_pendingCrossfadeReset.exchange(false, std::memory_order_acq_rel); }

    int samples() const { return m_crossfadeSamples.load(std::memory_order_relaxed); }
    int counter() const { return m_crossfadeCounter.load(std::memory_order_relaxed); }
    void setCounter(int val) { m_crossfadeCounter.store(val, std::memory_order_relaxed); }

    bool isActive() const { return counter() < samples(); }
    bool isComplete() const { return counter() >= samples(); }

    void copyToTemp(float** buffer, int numChannels, int numSamples)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
                m_crossfadeTempBuf[static_cast<size_t>(ch)][static_cast<size_t>(smp)] = buffer[ch][smp];
    }

    void captureOldOut(float** buffer, int numChannels, int numSamples)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
                m_crossfadeOldOut[static_cast<size_t>(ch)][static_cast<size_t>(smp)] = buffer[ch][smp];
    }

    void restoreInput(float** buffer, int numChannels, int numSamples)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int smp = 0; smp < numSamples; ++smp)
                buffer[ch][smp] = m_crossfadeTempBuf[static_cast<size_t>(ch)][static_cast<size_t>(smp)];
    }

    void fadeOutputs(float** buffer, int numChannels, int numSamples)
    {
        int cc = m_crossfadeCounter.load(std::memory_order_relaxed);
        int cfSamps = m_crossfadeSamples.load(std::memory_order_relaxed);
        for (int smp = 0; smp < numSamples; ++smp)
        {
            float g = std::min(1.0f, static_cast<float>(cc) / static_cast<float>(cfSamps));
            for (int ch = 0; ch < numChannels; ++ch)
                buffer[ch][smp] = m_crossfadeOldOut[static_cast<size_t>(ch)][static_cast<size_t>(smp)] * (1.0f - g)
                                + buffer[ch][smp] * g;
            ++cc;
        }
        m_crossfadeCounter.store(cc, std::memory_order_relaxed);
    }

private:
    std::atomic<int> m_crossfadeSamples{882};
    std::atomic<int> m_crossfadeCounter{0};
    std::atomic<bool> m_pendingCrossfadeReset{false};
    std::vector<std::vector<float>> m_crossfadeTempBuf;
    std::vector<std::vector<float>> m_crossfadeOldOut;
};
