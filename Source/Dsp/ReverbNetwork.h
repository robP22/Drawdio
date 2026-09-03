#pragma once

#include <array>
#include <cstddef>
#include <vector>

struct ReverbNetworkConfig
{
    double feedbackBase;
    double feedbackRange;
    std::array<int, 8> fdnTimes;
};

struct ReverbNetworkState
{
    struct DiffuserState
    {
        std::vector<float> buf;
        size_t writePtr = 0;
        float phase = 0.0f;
    };

    std::vector<float> fdnBuf[8];
    size_t fdnPtr[8];
    float fdnDampState[8];
    float fdnLfoPhase[8];
    float fdnDampCoeff[8];
    float lineMean = 1.0f;
    float decorrL;
    float decorrR;
    std::vector<float> reflectBuf[2];
    size_t reflectPtr;
    std::array<std::array<DiffuserState, 3>, 2> diffusers;
    float sizeScaleState = 0.65f;
    double sampleRate = 44100.0;
};

void prepareReverbNetwork(ReverbNetworkState& state, double sampleRate,
                          const ReverbNetworkConfig& config);
void resetReverbNetwork(ReverbNetworkState& state);
void prepareReverbNetworkBlock(ReverbNetworkState& state,
                               const ReverbNetworkConfig& config,
                               float decayNormalised);
void processReverbNetworkSample(float dryL, float dryR,
                                const ReverbNetworkConfig& config,
                                ReverbNetworkState& state,
                                float decayNormalised,
                                float sizeNormalised,
                                float& outL, float& outR);
