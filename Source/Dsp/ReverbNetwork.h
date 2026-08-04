#pragma once

#include <array>
#include <cstddef>
#include <vector>

struct ReverbNetworkConfig
{
    double feedbackBase;
    double feedbackRange;
    std::array<float, 4> combGains;
    float apCoeff;
    std::array<int, 4> combTimesMs;
    std::array<int, 2> apTimesMs;
};

struct ReverbNetworkState
{
    std::vector<float> combBuf[4];
    size_t combPtr[4];
    float combDampState[4];
    std::vector<float> apBuf[2];
    size_t apPtr[2];
    float decorrL;
    float decorrR;
    float combLfoPhase[4];
    std::vector<float> reflectBuf[2];
    size_t reflectPtr;
};

void prepareReverbNetwork(ReverbNetworkState& state, double sampleRate,
                          const ReverbNetworkConfig& config);
void resetReverbNetwork(ReverbNetworkState& state);
void processReverbNetworkSample(float dryL, float dryR,
                                const ReverbNetworkConfig& config,
                                ReverbNetworkState& state,
                                float decayNormalised,
                                float& outL, float& outR);
