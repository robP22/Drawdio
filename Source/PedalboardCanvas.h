#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "PedalComponent.h"

class DrawdioProcessor;

/**
    PedalboardCanvas manages the physical pedals and the routing cables between them.
    It handles both automatic (drawn) routing and manual interaction.
*/
class PedalboardCanvas : public juce::Component
{
public:
    PedalboardCanvas(DrawdioProcessor& processor);
    ~PedalboardCanvas() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateRouting(const std::vector<uint8_t>& routingOrder);
    void syncPedals();

    // Access to pedals for external sync
    PedalComponent* getPedal(int index) { return (index >= 0 && index < 6) ? m_pedalComponents[index].get() : nullptr; }

    // Interaction for manual routing
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    void rebuildFeltImage();
    void drawRoutingCables(juce::Graphics& g);
    void drawCablePlug(juce::Graphics& g, juce::Point<float> pos, bool isInput);
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
    std::array<std::unique_ptr<PedalComponent>, 6> m_pedalComponents;
    std::vector<uint8_t> m_routingOrder;

    juce::Image m_feltImage;
    juce::Rectangle<int> m_boardBounds;
    juce::Rectangle<int> m_feltBounds;

    // Cable dragging state
    bool m_isDraggingCable = false;
    juce::Point<float> m_dragStartPos;
    juce::Point<float> m_dragCurrentPos;
    int m_dragSrcJackIdx = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalboardCanvas)
};
