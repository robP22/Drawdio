#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "Core/DrawdioConstants.h"
#include "UI/EditorState.h"
#include "Core/EditorDesignMetrics.h"
#include "State/CanvasRoutingManager.h"
#include "State/ManualConnectionModel.h"
#include "UI/Pedalboard/PedalComponent.h"
#include "Core/Contracts/IResourceProvider.h"
#include "Resources/ScaledAssetProvider.h"
#include "UI/Theme/IThemeProvider.h"
#include "UI/Pedalboard/PedalboardLayout.h"
#include "UI/Pedalboard/CablePathBuilder.h"
#include "UI/Pedalboard/CableRenderer.h"
#include "UI/Pedalboard/JackHitMap.h"
#include "UI/Pedalboard/ManualRoutingController.h"

class PedalboardGrid : public juce::Component
{
public:
    struct Actions
    {
        std::function<void(int, DspModuleType)> setPedalType;
        std::function<void(int, int, float, float)> setKnob;
        std::function<void(int, int, bool)> setLink;
        std::function<void(int, int, float, float)> setLinkRange;
        std::function<void(const std::vector<uint8_t>&)> setManualRouting;
    };

    PedalboardGrid(const EditorUiSnapshot& initialState,
                   const IResourceProvider& resources,
                   const ScaledAssetProvider& assets,
                   const IThemeProvider& theme,
                   CanvasRoutingManager& routingManager,
                   Actions actions);
    ~PedalboardGrid() override = default;

    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    void updateRouting(const std::vector<uint8_t>& routingOrder);
    void syncPedals();
    void setViewState(const EditorUiSnapshot& state);
    void refreshAfterResize();
    void syncKnobLinkState(int slot, int knob, bool linked, float rangeMin, float rangeMax);

    PedalComponent* getPedal(int index) { return (index >= 0 && index < PedalSlotCount) ? m_pedalComponents[static_cast<size_t>(index)].get() : nullptr; }

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void restoreFromRouting(const std::vector<uint8_t>& routing);
    void rebuildCableCache();

    std::array<IComponentBounds*, PedalSlotCount> componentBounds() const
    {
        std::array<IComponentBounds*, PedalSlotCount> result;
        for (int i = 0; i < PedalSlotCount; ++i)
            result[static_cast<size_t>(i)] = m_pedalComponents[static_cast<size_t>(i)].get();
        return result;
    }

private:
    juce::Point<float> dawEntryPos() const
    {
        return {static_cast<float>(getWidth()) * 0.05f,
                static_cast<float>(getHeight()) * EditorDesignMetrics::Cable::JackHeightRatio * 0.5f};
    }

    juce::Point<float> dawExitPos() const
    {
        return {static_cast<float>(getWidth()) * 0.95f,
                static_cast<float>(getHeight()) * EditorDesignMetrics::Cable::JackHeightRatio * 0.5f};
    }

    void rebuildConnectionCables();

    juce::Path m_cachedInputPath;
    juce::Path m_cachedOutputPath;
    std::vector<CachedSplitCable> m_cachedConnectionPaths;

    const IResourceProvider& m_resources;
    const ScaledAssetProvider& m_assets;
    const IThemeProvider& m_theme;
    CanvasRoutingManager& m_routingManager;
    std::array<std::unique_ptr<PedalComponent>, PedalSlotCount> m_pedalComponents;

    ManualConnectionModel m_connectionModel;
    PedalboardLayout m_layout;
    CableRenderer m_renderer;
    JackHitMap m_jackMap;
    ManualRoutingController m_routingCtrl;
    Actions m_actions;
    std::array<DspModuleType, PedalSlotCount> m_pedalTypes{};
    std::array<std::array<bool, KnobsPerPedal>, PedalSlotCount> m_linked{};
    std::array<std::array<float, KnobsPerPedal>, PedalSlotCount> m_linkMins{};
    std::array<std::array<float, KnobsPerPedal>, PedalSlotCount> m_linkMaxs{};
    bool m_manualMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalboardGrid)
};
