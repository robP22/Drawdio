#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <vector>
#include "Core/DrawdioConstants.h"
#include "UI/EditorProcessorBridge.h"
#include "UI/EditorPresentationStore.h"
#include "UI/EditorViewBinder.h"

class PedalboardGrid;
class BottomControlBar;
class PedalboardHeader;
class AutomationPlayer;
class AutomationCompiler;
class PixelCanvasComponent;
class ColorPalette;

class EditorSyncController
{
public:
    EditorSyncController(EditorProcessorBridge& processor,
                         PedalboardGrid& pedalboardGrid,
                         BottomControlBar& bottomBar,
                         PedalboardHeader& pedalboardHeader,
                         AutomationPlayer& automationPlayer,
                         AutomationCompiler& automationCompiler,
                         PixelCanvasComponent& pixelCanvas,
                         ColorPalette& palette);

    void tick();
    bool needsRepaint() const { return m_needsRepaint; }
    void clearRepaintFlag() { m_needsRepaint = false; }
    void requestSync() { m_needsRepaint = true; }

    void setAutoEnvelopeDirty() { m_autoEnvelopeDirty = true; }
    void clearRoutingCache() { m_lastRoutingOrder.clear(); m_lastRestoredManualRouting.clear(); m_lastPedalTypes.fill(DspModuleType::BYPASS); }

private:
    void processPendingOperations();
    bool consumeUINotification();
    void applyFullConfigSync(bool& needsRepaint);
    void syncCompiledKnobs(bool& needsRepaint);
    void syncAutomation();
    void syncKnobAutomation();
    void syncKnobLinkIndicators();
    void refreshRoutingFromConfig();

    EditorProcessorBridge& m_processor;
    PedalboardGrid& m_pedalboardGrid;
    BottomControlBar& m_bottomBar;
    PedalboardHeader& m_pedalboardHeader;
    AutomationPlayer& m_automationPlayer;
    AutomationCompiler& m_automationCompiler;
    PixelCanvasComponent& m_pixelCanvas;
    ColorPalette& m_palette;
    EditorViewBinder m_viewBinder;
    EditorPresentationStore m_presentationStore;

    std::vector<uint8_t> m_lastRoutingOrder;
    std::vector<uint8_t> m_lastRestoredManualRouting;
    std::array<DspModuleType, PedalSlotCount> m_lastPedalTypes{};
    bool m_needsRepaint = false;
    bool m_autoEnvelopeDirty = true;
    uint32_t m_lastReportedDrops = 0;
};
