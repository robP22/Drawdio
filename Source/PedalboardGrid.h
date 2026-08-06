#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "Core/DrawdioConstants.h"
#include "State/CanvasRoutingManager.h"
#include "State/ManualConnectionModel.h"
#include "UI/Pedalboard/PedalComponent.h"
#include "Core/Contracts/IResourceProvider.h"
#include "UI/Theme/IThemeProvider.h"
#include "Core/Contracts/ProcessorInterfaces.h"
#include "UI/Pedalboard/PedalboardLayout.h"
#include "UI/Pedalboard/CablePathBuilder.h"
#include "UI/Pedalboard/CableRenderer.h"
#include "UI/Pedalboard/JackHitMap.h"
#include "UI/Pedalboard/ManualRoutingController.h"

class PedalboardGrid : public juce::Component
{
public:
    PedalboardGrid(IPedalboardModel& model,
                   const IResourceProvider& resources,
                   const IThemeProvider& theme,
                   CanvasRoutingManager& routingManager);
    ~PedalboardGrid() override = default;

    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    void updateRouting(const std::vector<uint8_t>& routingOrder);
    void syncPedals();

    PedalComponent* getPedal(int index) { return (index >= 0 && index < PedalSlotCount) ? m_pedalComponents[static_cast<size_t>(index)].get() : nullptr; }

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void clearInputOutputCables() { m_cachedInputPath.clear(); m_cachedOutputPath.clear(); }
    void clearEdges() { m_connectionModel.clear(); }
    void restoreFromRouting(const std::vector<uint8_t>& routing);
    void rebuildCableCache();
    void buildInputCableTo(int pedalSlot);
    void buildOutputCableFrom(int pedalSlot);

    std::array<IComponentBounds*, PedalSlotCount> componentBounds() const
    {
        std::array<IComponentBounds*, PedalSlotCount> result;
        for (int i = 0; i < PedalSlotCount; ++i)
            result[static_cast<size_t>(i)] = m_pedalComponents[static_cast<size_t>(i)].get();
        return result;
    }

private:
    juce::Point<float> dawEntryPos() const { return {static_cast<float>(getWidth()) * 0.05f, 0.0f}; }
    juce::Point<float> dawExitPos() const { return {static_cast<float>(getWidth()) * 0.95f, 0.0f}; }

    void rebuildConnectionCables();
    void buildSameRowCable(int srcIdx, int dstIdx, const juce::Point<float>& p1, const juce::Point<float>& p2);
    void buildAdjacentColumnCable(int srcIdx, int dstIdx, const juce::Point<float>& p1, const juce::Point<float>& p2);
    void buildDistantColumnCable(int srcIdx, int dstIdx, const juce::Point<float>& p1, const juce::Point<float>& p2);

    juce::Path m_cachedInputPath;
    juce::Path m_cachedOutputPath;
    std::vector<CachedSplitCable> m_cachedConnectionPaths;

    IPedalboardModel& m_model;
    const IResourceProvider& m_resources;
    const IThemeProvider& m_theme;
    CanvasRoutingManager& m_routingManager;
    std::array<std::unique_ptr<PedalComponent>, PedalSlotCount> m_pedalComponents;

    ManualConnectionModel m_connectionModel;
    PedalboardLayout m_layout;
    CableRenderer m_renderer;
    JackHitMap m_jackMap;
    ManualRoutingController m_routingCtrl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalboardGrid)
};
