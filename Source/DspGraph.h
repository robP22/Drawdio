#pragma once
#include <array>
#include <memory>
#include <vector>
#include <cstdint>
#include "Effects/DspEffect.h"

constexpr int MaxChainNodes = 8;

class DspGraph
{
public:
    DspGraph();
    ~DspGraph() = default;

    // Build the processing chain from effect types
    void buildChain(const std::vector<DspModuleType>& effectTypes);

    // Process audio through the graph
    void process(float** buffer, int numChannels, int numSamples);

    // Reset all effect states
    void reset();

    // Prepare all effects for playback
    void prepare(double sampleRate, int numChannels);

    // Get number of effects in chain
    int getChainLength() const { return m_chainLength; }

private:
    std::array<std::unique_ptr<DspEffect>, MaxChainNodes> m_effects;
    int m_chainLength = 0;

    // Crossfade support for dynamic reconfiguration
    std::array<std::unique_ptr<DspEffect>, MaxChainNodes> m_nextEffects;
    int m_crossfadeRemaining = 0;
    static constexpr int kCrossfadeSamples = 1024;
};