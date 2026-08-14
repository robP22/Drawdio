#include <JuceHeader.h>
#include "Effects/ConvolutionSpaceEffect.h"
#include <algorithm>
#include <cmath>
#include <random>

struct FftChannel
{
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftBuf;
    std::vector<std::vector<float>> dampGrid;
    std::vector<float> baseIr;
    std::vector<float> overflowBuf;
    std::vector<float> circBuf;
    size_t irLen = 0;
    size_t writePtr = 0;
};

static std::vector<float> generateSyntheticIR(double sampleRate, float decaySec)
{
    size_t irLen = static_cast<size_t>(sampleRate * decaySec);
    if (irLen < 16) irLen = 16;
    if (irLen > static_cast<size_t>(ConvolutionSpaceEffect::kFftSize / 2))
        irLen = static_cast<size_t>(ConvolutionSpaceEffect::kFftSize / 2);

    std::vector<float> ir(irLen, 0.0f);
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    ir[0] = 1.0f;

    for (size_t i = 1; i < irLen; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(irLen);
        float env = std::exp(-t * 4.0f);
        ir[i] = dist(rng) * env * 0.15f;
    }

    float peak = 0.0f;
    for (auto& v : ir) peak = std::max(peak, std::abs(v));
    if (peak > 0.0f)
        for (auto& v : ir) v /= peak * 1.2f;

    return ir;
}

ConvolutionSpaceEffect::ConvolutionSpaceEffect() : DspEffect(0) {}
ConvolutionSpaceEffect::~ConvolutionSpaceEffect() = default;

void ConvolutionSpaceEffect::prepare(double sampleRate, int numChannels)
{
    DspEffect::prepare(sampleRate, numChannels);
    m_channels.resize(static_cast<size_t>(numChannels));

    for (size_t ci = 0; ci < m_channels.size(); ++ci)
    {
        auto& ch = m_channels[ci];
        ch = std::make_unique<FftChannel>();
        auto& fc = *ch;
        fc.fft = std::make_unique<juce::dsp::FFT>(kFftOrder);
        fc.fftBuf.assign(static_cast<size_t>(kFftSize) * 2, 0.0f);
        fc.dampGrid.assign(static_cast<size_t>(kDampGridSize), std::vector<float>());
        for (auto& spectrum : fc.dampGrid)
            spectrum.assign(static_cast<size_t>(kFftSize), 0.0f);
        fc.baseIr = generateSyntheticIR(sampleRate, 0.8f);
        fc.irLen = fc.baseIr.size();
        fc.overflowBuf.assign(fc.irLen, 0.0f);
        fc.circBuf.assign(static_cast<size_t>(sampleRate * 1.0), 0.0f);
        fc.writePtr = 0;
        precomputeDampGrid(ci);
    }
}

void ConvolutionSpaceEffect::reset()
{
    for (auto& ch : m_channels)
    {
        if (!ch) continue;
        auto& fc = *ch;
        std::fill(fc.fftBuf.begin(), fc.fftBuf.end(), 0.0f);
        std::fill(fc.overflowBuf.begin(), fc.overflowBuf.end(), 0.0f);
        std::fill(fc.circBuf.begin(), fc.circBuf.end(), 0.0f);
        fc.writePtr = 0;
    }
}

void ConvolutionSpaceEffect::precomputeDampGrid(size_t chIdx)
{
    auto& fc = *m_channels[chIdx];

    for (int k = 0; k < kDampGridSize; ++k)
    {
        float damp = static_cast<float>(k) / static_cast<float>(kDampGridSize - 1);

        for (size_t i = 0; i < fc.irLen; ++i)
        {
            float dampScale = damp * 0.9f / static_cast<float>(fc.irLen);
            fc.fftBuf[i] = fc.baseIr[i] * (1.0f - dampScale * static_cast<float>(i));
        }
        std::fill(fc.fftBuf.begin() + static_cast<ptrdiff_t>(fc.irLen),
                  fc.fftBuf.begin() + kFftSize, 0.0f);
        std::fill(fc.fftBuf.begin() + kFftSize, fc.fftBuf.end(), 0.0f);

        fc.fft->performRealOnlyForwardTransform(fc.fftBuf.data());
        std::copy(fc.fftBuf.begin(), fc.fftBuf.begin() + kFftSize, fc.dampGrid[static_cast<size_t>(k)].begin());
    }
}

void ConvolutionSpaceEffect::processSubBlock(float** b, int offset, int subN, size_t chIdx, int gridIdx)
{
    auto& fc = *m_channels[chIdx];
    size_t irLen = fc.irLen;

    float* input = b[static_cast<int>(chIdx)] + offset;
    std::copy(input, input + subN, fc.fftBuf.begin());
    std::fill(fc.fftBuf.begin() + subN, fc.fftBuf.begin() + kFftSize, 0.0f);
    std::fill(fc.fftBuf.begin() + kFftSize, fc.fftBuf.end(), 0.0f);

    fc.fft->performRealOnlyForwardTransform(fc.fftBuf.data());

    const auto& irFreq = fc.dampGrid[static_cast<size_t>(gridIdx)];
    fc.fftBuf[0] *= irFreq[0];
    fc.fftBuf[1] *= irFreq[1];
    for (int i = 2; i < kFftSize; i += 2)
    {
        float re = fc.fftBuf[i] * irFreq[i] - fc.fftBuf[i + 1] * irFreq[i + 1];
        float im = fc.fftBuf[i] * irFreq[i + 1] + fc.fftBuf[i + 1] * irFreq[i];
        fc.fftBuf[i] = re;
        fc.fftBuf[i + 1] = im;
    }

    fc.fft->performRealOnlyInverseTransform(fc.fftBuf.data());

    float invN = 1.0f / static_cast<float>(kFftSize);
    int validLen = subN + static_cast<int>(irLen) - 1;
    if (validLen > kFftSize) validLen = kFftSize;
    for (int i = 0; i < validLen; ++i)
        fc.fftBuf[i] *= invN;

    int overlapLen = static_cast<int>(irLen) - 1;
    if (overlapLen > 0)
    {
        int addLen = std::min(overlapLen, validLen);
        for (int i = 0; i < addLen; ++i)
            fc.fftBuf[i] += fc.overflowBuf[static_cast<size_t>(i)];
    }

    std::copy(fc.fftBuf.begin(), fc.fftBuf.begin() + subN, input);

    if (overlapLen > 0)
    {
        int overflowStart = subN;
        int saveLen = std::min(overlapLen, validLen - subN);
        if (saveLen > 0)
            std::copy(fc.fftBuf.begin() + overflowStart,
                      fc.fftBuf.begin() + overflowStart + saveLen,
                      fc.overflowBuf.begin());
        else
            std::fill(fc.overflowBuf.begin(), fc.overflowBuf.begin() + static_cast<size_t>(overlapLen), 0.0f);
    }
}

void ConvolutionSpaceEffect::processBlockBruteForce(float** b, int c, int n, float damp)
{
    for (int ch = 0; ch < c; ++ch)
    {
        auto& fc = *m_channels[static_cast<size_t>(ch)];
        size_t circSize = fc.circBuf.size();
        if (circSize == 0) continue;

        float dampScale = damp * 0.9f;

        for (int s = 0; s < n; ++s)
        {
            float in = b[ch][s];
            if (!std::isfinite(in)) in = 0.0f;
            fc.circBuf[fc.writePtr] = in;

            float out = 0.0f;
            float invIrLen = 1.0f / static_cast<float>(fc.irLen);
            float dampMul = dampScale * invIrLen;
            size_t wp = fc.writePtr;

            for (size_t i = 0; i < fc.irLen && i < circSize; ++i)
            {
                size_t idx = wp >= i ? wp - i : wp + circSize - i;
                out += fc.circBuf[idx] * fc.baseIr[i] * (1.0f - dampMul * static_cast<float>(i));
            }

            b[ch][s] = out;
            fc.writePtr = (fc.writePtr + 1) % circSize;
        }
    }
}

void ConvolutionSpaceEffect::processSample(float** b, int c, int s, float effectParam)
{
    juce::ScopedNoDenormals noDenorm;
    float damp = effectParam;
    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& fc = *m_channels[static_cast<size_t>(ch)];
        size_t circSize = fc.circBuf.size();
        if (circSize == 0) continue;

        float inVal = b[ch][s];
        if (!std::isfinite(inVal)) inVal = 0.0f;
        fc.circBuf[fc.writePtr] = inVal;

        float out = 0.0f;
        float dampScale = damp * 0.9f / static_cast<float>(fc.irLen);
        for (size_t i = 0; i < fc.irLen && i < circSize; ++i)
        {
            size_t idx = fc.writePtr >= i ? fc.writePtr - i : fc.writePtr + circSize - i;
            out += fc.circBuf[idx] * fc.baseIr[i] * (1.0f - dampScale * static_cast<float>(i));
        }
        b[ch][s] = out;
        fc.writePtr = (fc.writePtr + 1) % circSize;
    }
}

void ConvolutionSpaceEffect::processBlock(float** b, int c, int n, const float* params)
{
    juce::ScopedNoDenormals noDenorm;
    float damp = params[3];
    int chCount = std::min(c, static_cast<int>(m_channels.size()));
    if (chCount == 0) return;

    int gridIdx = static_cast<int>(damp * static_cast<float>(kDampGridSize - 1) + 0.5f);
    gridIdx = juce::jlimit(0, kDampGridSize - 1, gridIdx);

    bool useFallback = false;
    for (int ch = 0; ch < chCount; ++ch)
    {
        auto& fc = *m_channels[static_cast<size_t>(ch)];
        if (fc.irLen == 0) { useFallback = true; break; }
        if (static_cast<size_t>(n) + fc.irLen > static_cast<size_t>(kFftSize))
            useFallback = true;
    }

    if (useFallback)
    {
        processBlockBruteForce(b, chCount, n, damp);
        float peak = 0.0f;
        for (int ch = 0; ch < chCount; ++ch)
            for (int s = 0; s < n; ++s)
                peak = std::max(peak, std::abs(b[ch][s]));
        m_hasTail = (peak > 1e-8f);
        return;
    }

    int subBlockStep = kFftSize - static_cast<int>(m_channels[0]->irLen);
    if (subBlockStep < 1) subBlockStep = 1;

    for (int ch = 0; ch < chCount; ++ch)
    {
        int processed = 0;
        while (processed < n)
        {
            int subN = std::min(n - processed, subBlockStep);
            processSubBlock(b, processed, subN, static_cast<size_t>(ch), gridIdx);
            processed += subN;
        }
    }

    float peak = 0.0f;
    for (int ch = 0; ch < chCount; ++ch)
        for (int s = 0; s < n; ++s)
            peak = std::max(peak, std::abs(b[ch][s]));
    m_hasTail = (peak > 1e-8f);
}
