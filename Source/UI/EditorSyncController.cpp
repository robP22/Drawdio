#include "EditorSyncController.h"
#include "../PedalboardGrid.h"
#include "UI/Controls/BottomControlBar.h"
#include "UI/Pedalboard/PedalboardHeader.h"
#include "State/AutomationPlayer.h"
#include "State/AutomationCompiler.h"
#include "UI/Canvas/PixelCanvasComponent.h"
#include "UI/Canvas/ColorPalette.h"

EditorSyncController::EditorSyncController(EditorProcessorBridge& processor,
                                           PedalboardGrid& pedalboardGrid,
                                           BottomControlBar& bottomBar,
                                           PedalboardHeader& pedalboardHeader,
                                           AutomationPlayer& automationPlayer,
                                           AutomationCompiler& automationCompiler,
                                           PixelCanvasComponent& pixelCanvas,
                                           ColorPalette& palette)
    : m_processor(processor),
      m_pedalboardGrid(pedalboardGrid),
      m_bottomBar(bottomBar),
      m_pedalboardHeader(pedalboardHeader),
      m_automationPlayer(automationPlayer),
      m_automationCompiler(automationCompiler),
      m_pixelCanvas(pixelCanvas),
      m_palette(palette),
      m_viewBinder(pedalboardGrid, bottomBar, pedalboardHeader, pixelCanvas, palette) {}

void EditorSyncController::tick()
{
    bool needsRepaint = false;

    processPendingOperations();

    syncCompiledKnobs(needsRepaint);
    syncAutomation();
    for (int slot = 0; slot < PedalSlotCount; ++slot)
        m_bottomBar.setPedalPeak(slot, m_processor.getPedalPeak(slot));

    if (consumeUINotification())
        applyFullConfigSync(needsRepaint);
    else
        refreshRoutingFromConfig();

    m_needsRepaint = (needsRepaint || m_needsRepaint);
}

void EditorSyncController::processPendingOperations()
{
    auto dropped = m_processor.getReleaseQueueDroppedCount();
    if (dropped > m_lastReportedDrops)
    {
        DBG("[Drawdio] ReleaseQueue overflow: " << dropped << " payload(s) dropped");
        m_lastReportedDrops = dropped;
    }
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

    const auto snapshot = m_processor.getUiSnapshot();
    m_presentationStore.apply(snapshot);
    m_viewBinder.apply(snapshot);
    m_bottomBar.syncPedalNames();
    refreshRoutingFromConfig();

    if (!m_pixelCanvas.isStrokeOpen() && !m_pixelCanvas.hasUndoData())
    {
        const auto& persisted = m_processor.getUndoData();
        if (!persisted.empty())
        {
            m_pixelCanvas.setGridData(m_processor.getGridData(), false);
            m_pixelCanvas.applyUndoData(persisted);
        }
    }
    else if (!m_pixelCanvas.isStrokeOpen())
    {
        m_processor.storeUndoData(m_pixelCanvas.captureUndoData());
    }

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

    m_pedalboardHeader.updateModeButton(m_processor.isManualMode());
}

void EditorSyncController::syncCompiledKnobs(bool& needsRepaint)
{
    uint32_t revBefore = m_processor.getConfigRevision();
    m_processor.consumeCompiledResultIfAvailable();
    if (m_processor.getConfigRevision() != revBefore)
    {
        needsRepaint = true;
    }
}

void EditorSyncController::syncAutomation()
{
    if (m_autoEnvelopeDirty)
    {
        m_autoEnvelopeDirty = false;
        auto envelope = m_automationCompiler.compile(m_pixelCanvas.getGridData());
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
    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        auto* pedal = m_pedalboardGrid.getPedal(slot);
        if (!pedal) continue;
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            if (!m_processor.isKnobLinked(slot, k))
                continue;
            const float rMin = m_processor.getKnobLinkRangeMin(slot, k);
            const float rMax = m_processor.getKnobLinkRangeMax(slot, k);
            float display = std::clamp(rMin + autoVal * (rMax - rMin), 0.0f, 1.0f);
            display = pedal->snapValue(k, display);
            pedal->setKnobValue(k, display);
        }
    }
    syncKnobLinkIndicators();
}

void EditorSyncController::syncKnobLinkIndicators()
{
    for (int slot = 0; slot < PedalSlotCount; ++slot)
        for (int knob = 0; knob < KnobsPerPedal; ++knob)
        {
            const bool linked = m_processor.isKnobLinked(slot, knob);
            const float rMin = m_processor.getKnobLinkRangeMin(slot, knob);
            const float rMax = m_processor.getKnobLinkRangeMax(slot, knob);
            m_pedalboardGrid.syncKnobLinkState(slot, knob, linked, rMin, rMax);
        }
}

void EditorSyncController::refreshRoutingFromConfig()
{
    bool pedalChanged = false;
    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        auto type = m_processor.getPedalSlot(slot);
        if (type != m_lastPedalTypes[static_cast<size_t>(slot)])
            pedalChanged = true;
        m_lastPedalTypes[static_cast<size_t>(slot)] = type;
    }
    if (pedalChanged)
    {
        const auto snapshot = m_processor.getUiSnapshot();
        m_pedalboardGrid.setViewState(snapshot);
        m_bottomBar.setViewState(snapshot);
        m_bottomBar.syncPedalNames();
        m_needsRepaint = true;
    }

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

    bool routingChanged = (routingOrder != m_lastRoutingOrder || m_lastRoutingOrder.empty());
    if (routingChanged)
    {
        m_lastRoutingOrder = routingOrder;
        m_pedalboardGrid.updateRouting(m_lastRoutingOrder);
        m_needsRepaint = true;
    }
}
