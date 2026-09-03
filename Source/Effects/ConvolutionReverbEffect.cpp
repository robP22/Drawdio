#include <JuceHeader.h>
#include "Effects/ConvolutionReverbEffect.h"
#include <algorithm>
#include <cmath>
#include <random>

struct ConvolutionChannel
{
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftBuf;         // kFftSize*2 workspace (real FFT packing)
    std::vector<float> inputSpectrum;  // kFftSize
    std::vector<float> inputBlock;     // kBlockLen
    std::vector<float> acc;            // partitionCount * kFftSize cascade accumulators
    float lpZ1 = 0.0f;
    float lpZ2 = 0.0f;
};

namespace
{
constexpr int kPartLen = ConvolutionReverbEffect::kBlockLen;

// Direct component + early reflections + exponentially-decaying diffusive
// noise, normalized so the tail's L2 norm is comparable to the input.
void generateBaseIR(double sampleRate, float* ir, size_t irLen, uint32_t seed, float directGain)
{
    juce::ignoreUnused(sampleRate);
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    ir[0] = directGain;
    static constexpr size_t kErOffsets[3] = { 137, 419, 883 };
    for (int e = 0; e < 3; ++e)
        if (kErOffsets[static_cast<size_t>(e)] < irLen)
            ir[kErOffsets[static_cast<size_t>(e)]] = directGain * 0.5f / static_cast<float>(e + 1);

    // Filtered-noise tail: a one-pole lowpass whose cutoff rolls 8k -> 1k
    // across the IR (a correlated, darkening wash instead of raw white noise,
    // which convolved to a static-like haze). The fixed envelope is mild so
    // the Size knob's RT60 scale governs the decay.
    float lp = 0.0f;
    for (size_t i = 1; i < irLen; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(irLen);
        const float cutoffHz = 8000.0f * std::pow(1000.0f / 8000.0f, t);
        const float alpha = 1.0f - std::exp(-6.2831853f * cutoffHz / static_cast<float>(sampleRate));
        lp += alpha * (dist(rng) - lp);
        ir[i] += lp * std::exp(-t * 0.8f) * 0.22f;
    }

    double energy = 0.0;
    for (size_t i = 1; i < irLen; ++i)
        energy += static_cast<double>(ir[i]) * ir[i];
    const float norm = static_cast<float>(std::sqrt(energy));
    if (norm > 1.0e-6f)
    {
        const float scale = 0.6f / norm;
        for (size_t i = 1; i < irLen; ++i)
            ir[i] *= scale;
    }
}
}

ConvolutionReverbEffect::ConvolutionReverbEffect() : DspEffect(0) {}
ConvolutionReverbEffect::~ConvolutionReverbEffect() = default;

void ConvolutionReverbEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);

    const size_t irLen = static_cast<size_t>(sampleRate * 0.8);
    const size_t padded = ((irLen + kPartLen - 1) / kPartLen) * kPartLen;
    m_partitionCount = static_cast<int>(padded / kPartLen);
    if (m_partitionCount < 1)
        m_partitionCount = 1;

    std::vector<float> irCorr(padded, 0.0f);
    std::vector<float> irDecorr(padded, 0.0f);
    generateBaseIR(sampleRate, irCorr.data(), padded, 0x12345678u, 0.30f);
    generateBaseIR(sampleRate, irDecorr.data(), padded, 0x9E3779B9u, 0.30f);

    juce::dsp::FFT scratch(kFftOrder);
    std::vector<float> spec(kFftSize * 2, 0.0f);
    m_corrSpectra.assign(static_cast<size_t>(m_partitionCount), std::vector<float>(kFftSize, 0.0f));
    m_decorrSpectra.assign(static_cast<size_t>(m_partitionCount), std::vector<float>(kFftSize, 0.0f));
    for (int p = 0; p < m_partitionCount; ++p)
    {
        std::fill(spec.begin(), spec.end(), 0.0f);
        for (size_t i = 0; i < kPartLen; ++i)
            spec[i] = irCorr[static_cast<size_t>(p) * kPartLen + i];
        scratch.performRealOnlyForwardTransform(spec.data());
        std::copy(spec.begin(), spec.begin() + kFftSize, m_corrSpectra[static_cast<size_t>(p)].begin());

        std::fill(spec.begin(), spec.end(), 0.0f);
        for (size_t i = 0; i < kPartLen; ++i)
            spec[i] = irDecorr[static_cast<size_t>(p) * kPartLen + i];
        scratch.performRealOnlyForwardTransform(spec.data());
        std::copy(spec.begin(), spec.begin() + kFftSize, m_decorrSpectra[static_cast<size_t>(p)].begin());
    }

    m_channels.resize(static_cast<size_t>(numChannels));
    for (auto& ch : m_channels)
    {
        ch = std::make_unique<ConvolutionChannel>();
        ch->fft = std::make_unique<juce::dsp::FFT>(kFftOrder);
        ch->fftBuf.assign(static_cast<size_t>(kFftSize) * 2, 0.0f);
        ch->inputSpectrum.assign(static_cast<size_t>(kFftSize), 0.0f);
        ch->inputBlock.assign(static_cast<size_t>(kBlockLen), 0.0f);
        ch->acc.assign(static_cast<size_t>(m_partitionCount + 1) * kBlockLen, 0.0f);
        ch->lpZ1 = 0.0f;
        ch->lpZ2 = 0.0f;
    }
    m_scales.assign(static_cast<size_t>(m_partitionCount), 1.0f);
    m_dampCutoff = -1.0f;
    m_hasTail = false;
}

void ConvolutionReverbEffect::reset()
{
    for (auto& ch : m_channels)
    {
        if (!ch) continue;
        std::fill(ch->fftBuf.begin(), ch->fftBuf.end(), 0.0f);
        std::fill(ch->inputSpectrum.begin(), ch->inputSpectrum.end(), 0.0f);
        std::fill(ch->inputBlock.begin(), ch->inputBlock.end(), 0.0f);
        std::fill(ch->acc.begin(), ch->acc.end(), 0.0f);
        ch->lpZ1 = 0.0f;
        ch->lpZ2 = 0.0f;
    }
    m_hasTail = false;
}

void ConvolutionReverbEffect::processSample(float** b, int c, int s, float effectParam)
{
    float params[4] = { 0.5f, 0.5f, 0.5f, effectParam };
    float* sub[2] = { b[0] + s, (c > 1) ? b[1] + s : nullptr };
    processBlock(sub, c, 1, params);
}

void ConvolutionReverbEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    const float sizeP = std::max(0.0f, std::min(1.0f, params[1]));
    const float widthP = std::max(0.0f, std::min(1.0f, params[2]));
    const float dampP = std::max(0.0f, std::min(1.0f, params[3]));

    const float sr = static_cast<float>(m_sampleRate);
    const float rt60 = 0.15f + sizeP * 1.35f;
    const float decayPerPartition = std::exp(-6.907755f * static_cast<float>(kPartLen) / (rt60 * sr));
    m_scales[0] = 1.0f;
    for (int p = 1; p < m_partitionCount; ++p)
        m_scales[static_cast<size_t>(p)] = m_scales[static_cast<size_t>(p - 1)] * decayPerPartition;

    const float cutoff = 20000.0f * std::pow(1200.0f / 20000.0f, dampP);
    if (std::abs(cutoff - m_dampCutoff) > std::abs(m_dampCutoff) * 0.05f || m_dampCutoff < 0.0f)
    {
        m_dampB0Prev = m_dampB0; m_dampB1Prev = m_dampB1; m_dampB2Prev = m_dampB2;
        m_dampA1Prev = m_dampA1; m_dampA2Prev = m_dampA2;
        const float w0 = 6.2831853f * cutoff / sr;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) * 0.70710678f;
        const float invA = 1.0f / (1.0f + alpha);
        m_dampB0 = (1.0f - cosw) * 0.5f * invA;
        m_dampB1 = (1.0f - cosw) * invA;
        m_dampB2 = m_dampB0;
        m_dampA1 = -2.0f * cosw * invA;
        m_dampA2 = (1.0f - alpha) * invA;
        m_dampCutoff = cutoff;
    }

    const int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& pc = *m_channels[static_cast<size_t>(ch)];
        const bool isRight = (ch == 1);

        for (int off = 0; off < n; off += kBlockLen)
        {
            const int subN = std::min(n - off, kBlockLen);

            for (int s = 0; s < subN; ++s)
            {
                float x = b[ch][off + s];
                if (!std::isfinite(x)) x = 0.0f;
                const float t = (subN > 1) ? static_cast<float>(s) / static_cast<float>(subN) : 0.0f;
                const float b0 = m_dampB0Prev + (m_dampB0 - m_dampB0Prev) * t;
                const float b1 = m_dampB1Prev + (m_dampB1 - m_dampB1Prev) * t;
                const float b2 = m_dampB2Prev + (m_dampB2 - m_dampB2Prev) * t;
                const float a1 = m_dampA1Prev + (m_dampA1 - m_dampA1Prev) * t;
                const float a2 = m_dampA2Prev + (m_dampA2 - m_dampA2Prev) * t;
                const float lp = b0 * x + pc.lpZ1;
                pc.lpZ1 = b1 * x - a1 * lp + pc.lpZ2;
                pc.lpZ2 = b2 * x - a2 * lp;
                pc.inputBlock[static_cast<size_t>(s)] = lp;
            }
            for (int s = subN; s < kBlockLen; ++s)
                pc.inputBlock[static_cast<size_t>(s)] = 0.0f;

            std::fill(pc.fftBuf.begin(), pc.fftBuf.end(), 0.0f);
            std::copy(pc.inputBlock.begin(), pc.inputBlock.end(), pc.fftBuf.begin());
            pc.fft->performRealOnlyForwardTransform(pc.fftBuf.data());
            std::copy(pc.fftBuf.begin(), pc.fftBuf.begin() + kFftSize, pc.inputSpectrum.begin());

            for (int p = 0; p < m_partitionCount; ++p)
            {
                if (m_scales[static_cast<size_t>(p)] < 1.0e-4f)
                    break;

                const auto& base = m_corrSpectra[static_cast<size_t>(p)];
                const auto& alt = m_decorrSpectra[static_cast<size_t>(p)];
                const bool blend = isRight && widthP > 0.001f;
                const float scale = m_scales[static_cast<size_t>(p)];

                std::copy(pc.inputSpectrum.begin(), pc.inputSpectrum.end(), pc.fftBuf.begin());
                if (blend)
                {
                    pc.fftBuf[0] = pc.fftBuf[0] * (base[0] * (1.0f - widthP) + alt[0] * widthP) * scale;
                    pc.fftBuf[1] = pc.fftBuf[1] * (base[1] * (1.0f - widthP) + alt[1] * widthP) * scale;
                    for (int i = 2; i < kFftSize; i += 2)
                    {
                        const float re = base[i] * (1.0f - widthP) + alt[i] * widthP;
                        const float im = base[i + 1] * (1.0f - widthP) + alt[i + 1] * widthP;
                        const float xr = pc.fftBuf[i], xi = pc.fftBuf[i + 1];
                        pc.fftBuf[i] = (xr * re - xi * im) * scale;
                        pc.fftBuf[i + 1] = (xr * im + xi * re) * scale;
                    }
                }
                else
                {
                    for (int i = 0; i < kFftSize; i += 2)
                    {
                        const float re = base[i], im = base[i + 1];
                        const float xr = pc.fftBuf[i], xi = pc.fftBuf[i + 1];
                        pc.fftBuf[i] = (xr * re - xi * im) * scale;
                        pc.fftBuf[i + 1] = (xr * im + xi * re) * scale;
                    }
                }

                pc.fft->performRealOnlyInverseTransform(pc.fftBuf.data());

                const float nextScale = (p + 1 < m_partitionCount)
                    ? m_scales[static_cast<size_t>(p + 1)] : 0.0f;
                const float nextNextScale = (p + 2 < m_partitionCount)
                    ? m_scales[static_cast<size_t>(p + 2)] : 0.0f;
                float* accP = pc.acc.data() + static_cast<size_t>(p) * kBlockLen;
                float* accPNext = pc.acc.data() + static_cast<size_t>(p + 1) * kBlockLen;
                for (int k = 0; k < kBlockLen; ++k)
                {
                    const float ramp0 = scale + (nextScale - scale) * static_cast<float>(k) / kBlockLen;
                    const float ramp1 = nextScale + (nextNextScale - nextScale) * static_cast<float>(k) / kBlockLen;
                    accP[k] += pc.fftBuf[static_cast<size_t>(k)] * ramp0;
                    accPNext[k] += pc.fftBuf[static_cast<size_t>(kBlockLen + k)] * ramp1;
                }
            }

            for (int k = 0; k < subN; ++k)
            {
                float out = pc.acc[static_cast<size_t>(k)];
                if (!std::isfinite(out)) out = 0.0f;
                b[ch][off + k] = out;
                m_hasTail = m_hasTail || (std::abs(out) > 1.0e-8f);
            }

            for (int p = 0; p < m_partitionCount; ++p)
            {
                float* accP = pc.acc.data() + static_cast<size_t>(p) * kBlockLen;
                std::copy(accP + kBlockLen, accP + 2 * kBlockLen, accP);
            }
            std::fill(pc.acc.data() + static_cast<size_t>(m_partitionCount) * kBlockLen,
                      pc.acc.data() + static_cast<size_t>(m_partitionCount + 1) * kBlockLen, 0.0f);
        }
    }
}
