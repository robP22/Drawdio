#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include "Core/DrawdioConstants.h"
#include "Core/Contracts/IComponentBounds.h"

struct JackInfo
{
    int pedalIdx;
    bool isInput;
    juce::Point<float> pos;
};

class JackHitMap
{
public:
    static constexpr int kJackCount = PedalSlotCount * 2 + 2;

    float radius() const { return m_radius; }

    void refresh(const std::array<IComponentBounds*, PedalSlotCount>& pedals,
                 juce::Point<float> dawEntry, juce::Point<float> dawExit,
                 juce::Rectangle<int> gridBounds)
    {
        const float gs = static_cast<float>(std::min(gridBounds.getWidth(), gridBounds.getHeight()));
        m_radius = std::min(24.0f, std::max(12.0f, gs * (24.0f / 770.0f)));
        for (int i = 0; i < PedalSlotCount; ++i)
        {
            m_jacks[static_cast<size_t>(i) * 2]     = { i, true,  pedals[static_cast<size_t>(i)]->getInputJackPos() };
            m_jacks[static_cast<size_t>(i) * 2 + 1] = { i, false, pedals[static_cast<size_t>(i)]->getOutputJackPos() };
        }
        m_jacks[static_cast<size_t>(PedalSlotCount * 2)]     = { -1, false, dawEntry };
        m_jacks[static_cast<size_t>(PedalSlotCount * 2 + 1)] = { -2, true,  dawExit };
    }

    int findAt(juce::Point<float> pos, float radius) const
    {
        int best = -1;
        float bestDist = radius;
        for (int i = 0; i < kJackCount; ++i)
        {
            float d = m_jacks[static_cast<size_t>(i)].pos.getDistanceFrom(pos);
            if (d <= bestDist) { bestDist = d; best = i; }
        }
        return best;
    }

    const JackInfo& get(int index) const { return m_jacks[static_cast<size_t>(index)]; }

private:
    std::array<JackInfo, kJackCount> m_jacks{};
    float m_radius = 24.0f;
};
