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
    std::vector<float> fdnBuf[8];
    size_t fdnPtr[8];
    float fdnDampState[8];
    float fdnLfoPhase[8];
    float decorrL;
    float decorrR;
    std::vector<float> reflectBuf[2];
    size_t reflectPtr;
    double sampleRate = 44100.0;
};

void prepareReverbNetwork(ReverbNetworkState& state, double sampleRate,
                          const ReverbNetworkConfig& config);
void resetReverbNetwork(ReverbNetworkState& state);
void processReverbNetworkSample(float dryL, float dryR,
                                const ReverbNetworkConfig& config,
                                ReverbNetworkState& state,
                                float decayNormalised,
                                float& outL, float& outR);
