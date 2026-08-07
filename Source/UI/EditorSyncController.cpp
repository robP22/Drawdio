#include "EditorSyncController.h"
#include "../PedalboardGrid.h"
#include "UI/Controls/BottomControlBar.h"
#include "State/AutomationPlayer.h"
#include "State/AutomationCompiler.h"
#include "UI/Canvas/PixelCanvasComponent.h"

EditorSyncController::EditorSyncController(IConfigConsumer& processor,
                                           PedalboardGrid& pedalboardGrid,
                                           BottomControlBar& bottomBar,
                                           AutomationPlayer& automationPlayer,
                                           AutomationCompiler& automationCompiler,
                                           PixelCanvasComponent& pixelCanvas)
    : m_processor(processor),
      m_pedalboardGrid(pedalboardGrid),
      m_bottomBar(bottomBar),
      m_automationPlayer(automationPlayer),
      m_automationCompiler(automationCompiler),
      m_pixelCanvas(pixelCanvas) {}

void EditorSyncController::tick()
{
    bool needsRepaint = false;

    processPendingOperations();

    syncCompiledKnobs(needsRepaint);
    syncAutomation();

    if (consumeUINotification())
        applyFullConfigSync(needsRepaint);
    else
        refreshRoutingFromConfig();

    m_needsRepaint = (needsRepaint || m_needsRepaint);
}

void EditorSyncController::processPendingOperations()
{
    m_processor.drainReleaseQueue();
    m_processor.tryApplyDeferredConfig();
}

bool EditorSyncController::consumeUINotification()
{
    return m_processor.consumeUINotification();
}

void EditorSyncController::applyFullConfigSync(bool& needsRepaint)
{
    needsRepaint = true;

    m_pedalboardGrid.syncPedals();
    m_bottomBar.syncPedalNames();
    m_bottomBar.syncGainKnobs();
    refreshRoutingFromConfig();

    m_processor.storeUndoData(m_pixelCanvas.captureUndoData());
    m_pixelCanvas.setGridData(m_processor.getGridData());
    m_pixelCanvas.applyUndoData(m_processor.getUndoData());

    {
        int bars = m_processor.getBarCount();
        m_bottomBar.getAutomationDisplay().setBarCount(bars);
        m_bottomBar.updateBarsButton(bars);
        m_automationPlayer.setBarCount(bars);
    }
    {
        int start = m_processor.getSectionStart();
        m_bottomBar.getAutomationDisplay().setSectionStart(start);
        m_automationPlayer.setSectionStartBar(start);
    }

    m_bottomBar.updateManualButton(m_processor.isManualMode());
}

void EditorSyncController::syncCompiledKnobs(bool& needsRepaint)
{
    uint32_t revBefore = m_processor.getConfigRevision();
    m_processor.consumeCompiledResultIfAvailable();
    if (m_processor.getConfigRevision() != revBefore)
    {
        m_didConsumeResult = true;
        needsRepaint = true;
        if (!m_processor.isManualMode())
        {
            auto& syncData = m_processor.getLastConfigSync();
            for (auto& param : syncData.parameters)
                applyParameterToPedal(param, syncData.routingSlotOrder);
        }
    }
}

void EditorSyncController::applyParameterToPedal(
    const ParameterDescriptor& param,
    const std::vector<uint8_t>& routingSlotOrder)
{
    auto chainPos = param.targetDspNodeRegister;
    if (chainPos >= static_cast<int>(routingSlotOrder.size()))
        return;
    int slotIdx = routingSlotOrder[static_cast<size_t>(chainPos)];
    int token = static_cast<int>(param.parameterToken);
    if (auto* pedal = m_pedalboardGrid.getPedal(slotIdx))
    {
        if (m_processor.isParamOverridden(slotIdx, token))
        {
            float display = m_processor.getKnobDisplayValue(slotIdx, token, param.currentValue);
            pedal->setKnobValue(token, display);
            m_processor.storeParameterValue(slotIdx, token, display);
        }
        else
        {
            pedal->setKnobValue(token, param.currentValue);
            m_processor.storeParameterValue(slotIdx, token, param.currentValue);
        }
    }
}

void EditorSyncController::syncAutomation()
{
    if (m_autoEnvelopeDirty)
    {
        m_autoEnvelopeDirty = false;
        std::vector<DspModuleType> slots(PedalSlotCount);
        for (int i = 0; i < PedalSlotCount; ++i)
            slots[i] = m_processor.getPedalSlot(i);
        auto envelope = m_automationCompiler.compile(
            m_pixelCanvas.getGridData(), slots);
        m_automationPlayer.setEnvelope(envelope);
        m_bottomBar.getAutomationDisplay().setEnvelope(envelope);
    }

    float bpm = m_processor.getPlayHeadBpm();
    double ppq = m_processor.getPlayHeadPpq();
    bool playing = m_processor.isPlayHeadPlaying();
    if (!playing)
        m_processor.resetPedalPeaks();
    m_automationPlayer.tick(ppq, bpm, playing);
    m_processor.setAutomationValue(m_automationPlayer.getValue());
    m_bottomBar.getAutomationDisplay().setPlayheadTime(m_automationPlayer.getPlayheadTime());
    syncKnobAutomation();
}

void EditorSyncController::syncKnobAutomation()
{
    float autoVal = m_automationPlayer.getValue();
    auto knobVals = m_processor.getKnobValues();
    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        auto* pedal = m_pedalboardGrid.getPedal(slot);
        if (!pedal) continue;
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            if (!m_processor.isKnobLinked(slot, k))
                continue;
            size_t idx = static_cast<size_t>(slot * KnobsPerPedal + k);
            float strength = m_processor.getKnobLinkStrength(slot, k);
            float display = std::max(0.0f, std::min(1.0f, knobVals[idx] * (1.0f - strength) + autoVal * strength));
            pedal->setKnobValue(k, display);
        }
    }
}

void EditorSyncController::refreshRoutingFromConfig()
{
    if (m_processor.isManualMode())
    {
        const auto& routing = m_processor.getManualRouting();
        if (routing != m_lastRestoredManualRouting)
        {
            m_lastRestoredManualRouting = routing;
            m_pedalboardGrid.restoreFromRouting(routing);
            m_needsRepaint = true;
        }
        return;
    }
    const auto& routingOrder = m_processor.getLastConfigSync().routingSlotOrder;
    if (routingOrder != m_lastRoutingOrder || m_lastRoutingOrder.empty() || m_didConsumeResult)
    {
        m_didConsumeResult = false;
        m_lastRoutingOrder = routingOrder;
        m_pedalboardGrid.updateRouting(m_lastRoutingOrder);
        m_needsRepaint = true;
    }
}
