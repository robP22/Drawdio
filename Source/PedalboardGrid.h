#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "CanvasRoutingManager.h"
#include "ManualConnectionModel.h"
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

    // Manual mode cable path helpers — called from PluginEditor
    void clearInputOutputCables() { m_cachedInputPath.clear(); m_cachedOutputPath.clear(); }
    void clearEdges() { m_connectionModel.clear(); }
    void rebuildCableCache();
    void buildInputCableTo(int pedalSlot);
    void buildOutputCableFrom(int pedalSlot);

private:
    // Position helpers
    juce::Point<float> dawEntryPos() const { return {static_cast<float>(getWidth()) * 0.05f, 0.0f}; }
    juce::Point<float> dawExitPos() const { return {static_cast<float>(getWidth()) * 0.95f, 0.0f}; }

    // Helpers for jack detection
    struct JackInfo {
        int pedalIdx;
        bool isInput;
        juce::Point<float> pos;
    };
    int findJackAt(juce::Point<float> pos, float radius) const;

    void drawRoutingCables(juce::Graphics& g);
    void drawActiveDraggingCable(juce::Graphics& g);
    void drawGrabbedCable(juce::Graphics& g);
    void drawInputCable(juce::Graphics& g);
    void drawOutputCable(juce::Graphics& g);
    void removeGrabbedEdge();
    void reconnectGrabbedCable(const JackInfo& dst);

    // Cached cable paths — rebuilt when routing changes
    struct CachedSplitCable {
        juce::Path left;
        juce::Path right;
    };
    void refreshJacks();

    juce::Path m_cachedInputPath;
    juce::Path m_cachedOutputPath;
    std::vector<CachedSplitCable> m_cachedConnectionPaths;
    std::array<JackInfo, PedalSlotCount * 2 + 2> m_cachedJacks;

    DrawdioProcessor& audioProcessor;
    const ResourceManager& m_resources;
    const IThemeProvider& m_theme;
    CanvasRoutingManager& m_routingManager;
    std::array<std::unique_ptr<PedalComponent>, PedalSlotCount> m_pedalComponents;

    // Cable connection model
    // Cable connection model
    ManualConnectionModel m_connectionModel;

    enum class DragMode { None, NewCable, GrabCable };
    DragMode m_dragMode = DragMode::None;
    juce::Point<float> m_dragStartPos;
    juce::Point<float> m_dragCurrentPos;
    int m_dragSrcJackIdx = -1;

    // Cable grab state — when picking up an existing cable from its jack
    int m_grabbedEdgeIndex = -1;       // -1=input cable, -2=output cable, >=0=connection edge index
    bool m_grabbingSrcEnd = false;     // true=grabbing output/source end, false=grabbing input/dest end
    juce::Point<float> m_anchoredPos;  // position of the non-dragged (still connected) jack
    int m_grabbedSrcSlot = -1;         // original source pedal slot of the grabbed cable
    int m_grabbedDstSlot = -1;         // original dest pedal slot of the grabbed cable

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalboardGrid)
};
