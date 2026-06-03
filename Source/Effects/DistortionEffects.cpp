#include "Effects/DistortionEffects.h"

#include <cmath>

void WaveshaperEffect::processSample(float** b, int c, int s, float effectParam)
{
    float drive = effectParam;
    float clip = 0.5f + drive * 0.5f;
    if (drive < 0.001f) drive = 0.001f;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        b[ch][s] = (2.0f / 3.14159265f) * std::atan(x * drive * 5.0f) * clip;
    }
}

void SoftDistortionEffect::processSample(float** b, int c, int s, float effectParam)
{
    float d = 1.0f + effectParam * 4.0f;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        float absX = std::abs(x);
        if (absX < 0.333f)
            b[ch][s] = 2.0f * x * d;
        else if (absX < 0.667f)
            b[ch][s] = (3.0f - (2.0f - 3.0f * absX) * (2.0f - 3.0f * absX)) / 3.0f * ((x > 0.0f) ? 1.0f : -1.0f) * d;
        else
            b[ch][s] = ((x > 0.0f) ? 1.0f : -1.0f) * d;
    }
}

void WavefolderEffect::processSample(float** b, int c, int s, float effectParam)
{
    float d = 1.0f + effectParam * 9.0f;

    for (int ch = 0; ch < c; ++ch)
    {
        float x = b[ch][s];
        float folded = std::sin(x * d * 3.14159265f);
        float norm = 1.0f + d * 0.3f;
        b[ch][s] = folded / norm;
    }
}
