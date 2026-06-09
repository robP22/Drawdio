#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "CanvasRoutingManager.h"
#include "PedalComponent.h"
#include "ResourceManager.h"
#include "IThemeProvider.h"

class DrawdioProcessor;

/**
    PedalboardGrid manages the physical pedals and the routing cables between them.
    It handles both automatic (drawn) routing and manual interaction.
*/
class PedalboardGrid : public juce::Component
{
public:
    PedalboardGrid(DrawdioProcessor& processor,
                   const ResourceManager& resources,
                   const IThemeProvider& theme,
                   CanvasRoutingManager& routingManager);
    ~PedalboardGrid() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateRouting(const std::vector<uint8_t>& routingOrder);
    void syncPedals();

    // Access to pedals for external sync
    PedalComponent* getPedal(int index) { return (index >= 0 && index < PedalSlotCount) ? m_pedalComponents[static_cast<size_t>(index)].get() : nullptr; }

    // Interaction for manual routing
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    void drawRoutingCables(juce::Graphics& g);
    void drawActiveDraggingCable(juce::Graphics& g);

    // Helpers for jack detection
    struct JackInfo {
        int pedalIdx;
        bool isInput;
        juce::Point<float> pos;
    };
    std::vector<JackInfo> getJacks() const;
    int findJackAt(juce::Point<float> pos, float radius) const;

    DrawdioProcessor& audioProcessor;
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
    CanvasRoutingManager& m_routingManager;
    std::array<std::unique_ptr<PedalComponent>, PedalSlotCount> m_pedalComponents;

    // Cable dragging state
    bool m_isDraggingCable = false;
    juce::Point<float> m_dragStartPos;
    juce::Point<float> m_dragCurrentPos;
    int m_dragSrcJackIdx = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalboardGrid)
};
