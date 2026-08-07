#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <vector>
#include "Core/DrawdioConstants.h"
#include "Core/Contracts/IConfigConsumer.h"

class PedalboardGrid;
class BottomControlBar;
class AutomationPlayer;
class AutomationCompiler;
class PixelCanvasComponent;

class EditorSyncController
{
public:
    EditorSyncController(IConfigConsumer& processor,
                         PedalboardGrid& pedalboardGrid,
                         BottomControlBar& bottomBar,
                         AutomationPlayer& automationPlayer,
                         AutomationCompiler& automationCompiler,
                         PixelCanvasComponent& pixelCanvas);

    void tick();
    bool needsRepaint() const { return m_needsRepaint; }
    void clearRepaintFlag() { m_needsRepaint = false; }

    void setAutoEnvelopeDirty() { m_autoEnvelopeDirty = true; }
    void clearRoutingCache() { m_lastRoutingOrder.clear(); m_lastRestoredManualRouting.clear(); }

private:
    void processPendingOperations();
    bool consumeUINotification();
    void applyFullConfigSync(bool& needsRepaint);
    void syncCompiledKnobs(bool& needsRepaint);
    void syncAutomation();
    void syncKnobAutomation();
    void refreshRoutingFromConfig();
    void applyParameterToPedal(const ParameterDescriptor& param, const std::vector<uint8_t>& routingSlotOrder);

    IConfigConsumer& m_processor;
    PedalboardGrid& m_pedalboardGrid;
    BottomControlBar& m_bottomBar;
    AutomationPlayer& m_automationPlayer;
    AutomationCompiler& m_automationCompiler;
    PixelCanvasComponent& m_pixelCanvas;

    std::vector<uint8_t> m_lastRoutingOrder;
    std::vector<uint8_t> m_lastRestoredManualRouting;
    bool m_needsRepaint = false;
    bool m_autoEnvelopeDirty = true;
    bool m_didConsumeResult = false;
};
