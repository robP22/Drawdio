#include "EditorProcessorBridge.h"

#include <algorithm>
#include <utility>

#include "PluginProcessor.h"

EditorProcessorBridge::EditorProcessorBridge(DrawdioProcessor& processor)
    : m_processor(processor)
{
    m_processor.addEditorStateListener(this);
}

EditorProcessorBridge::~EditorProcessorBridge()
{
    detach();
}

void EditorProcessorBridge::detach()
{
    if (!m_attached)
        return;
    m_processor.removeEditorStateListener(this);
    m_attached = false;
}

void EditorProcessorBridge::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (m_attached && onStateChanged)
        onStateChanged();
}

EditorUiSnapshot EditorProcessorBridge::getUiSnapshot() const
{
    EditorUiSnapshot result;
    result.gridData = m_processor.getGridData();
    result.overrideMask = m_processor.getParamOverrideMask();
    result.barCount = static_cast<uint8_t>(m_processor.getBarCount());
    result.sectionStartBar = static_cast<uint8_t>(m_processor.getSectionStart());
    result.manualMode = m_processor.isManualMode();
    result.inputGain = m_processor.getInputGain();
    result.outputGain = m_processor.getOutputGain();
    result.inputMeter = m_processor.getInputMeterLevel();
    result.outputMeter = m_processor.getOutputMeterLevel();
    result.playHeadBpm = m_processor.getPlayHeadBpm();
    result.playHeadPpq = m_processor.getPlayHeadPpq();
    result.playHeadPlaying = m_processor.isPlayHeadPlaying();
    result.pendingConfig = m_processor.hasPendingConfig();
    result.configurationRevision = m_processor.getConfigRevision();
    result.sessionRevision = m_processor.getSessionRevision();
    result.releaseQueueDrops = m_processor.getReleaseQueueDroppedCount();
    result.session = m_processor.getEditorSessionState();

    const auto& manualRouting = m_processor.getManualRouting();
    result.manualRoutingSize = static_cast<uint8_t>(std::min(manualRouting.size(), result.manualRouting.size()));
    for (int i = 0; i < result.manualRoutingSize; ++i)
        result.manualRouting[static_cast<size_t>(i)] = manualRouting[static_cast<size_t>(i)];

    for (int slot = 0; slot < PedalSlotCount; ++slot)
    {
        auto& pedal = result.pedals[static_cast<size_t>(slot)];
        pedal.type = m_processor.getPedalSlot(slot);
        pedal.gain = m_processor.getPedalGain(slot);
        pedal.peak = m_processor.getPedalPeak(slot);
        for (int knob = 0; knob < KnobsPerPedal; ++knob)
        {
            pedal.linked[static_cast<size_t>(knob)] = m_processor.isKnobLinked(slot, knob);
            pedal.linkStrength[static_cast<size_t>(knob)] = m_processor.getKnobLinkStrength(slot, knob);
            pedal.linkRangeMins[static_cast<size_t>(knob)] = m_processor.getKnobLinkRangeMin(slot, knob);
            pedal.linkRangeMaxs[static_cast<size_t>(knob)] = m_processor.getKnobLinkRangeMax(slot, knob);
        }
    }

    const auto cacheValues = m_processor.getKnobValues();
    result.knobValues = cacheValues;
    for (int slot = 0; slot < PedalSlotCount; ++slot)
        for (int knob = 0; knob < KnobsPerPedal; ++knob)
            result.pedals[static_cast<size_t>(slot)].knobValues[static_cast<size_t>(knob)] =
                cacheValues[static_cast<size_t>(slot * KnobsPerPedal + knob)];

    const auto& sync = m_processor.getLastConfigSync();
    result.routingSize = static_cast<uint8_t>(std::min(sync.routingSlotOrder.size(), result.routingOrder.size()));
    for (int i = 0; i < result.routingSize; ++i)
        result.routingOrder[static_cast<size_t>(i)] = sync.routingSlotOrder[static_cast<size_t>(i)];

    if (!result.manualMode)
    {
        for (const auto& param : sync.parameters)
        {
            const auto chainPos = static_cast<size_t>(param.targetDspNodeRegister);
            if (chainPos >= sync.routingSlotOrder.size() || param.parameterToken >= KnobsPerPedal)
                continue;

            const int slot = sync.routingSlotOrder[chainPos];
            const int knob = static_cast<int>(param.parameterToken);
            float value = param.currentValue;
            if (m_processor.isParamOverridden(slot, knob))
                value = m_processor.getKnobDisplayValue(slot, knob, value);

            result.knobValues[static_cast<size_t>(slot * KnobsPerPedal + knob)] = value;
            result.pedals[static_cast<size_t>(slot)].knobValues[static_cast<size_t>(knob)] = value;
        }
    }

    return result;
}

void EditorProcessorBridge::setKnobLink(int slot, int knob, bool linked) { m_processor.setKnobLink(slot, knob, linked); }
void EditorProcessorBridge::setKnobLinkRange(int slot, int knob, float rangeMin, float rangeMax) { m_processor.setKnobLinkRange(slot, knob, rangeMin, rangeMax); }
float EditorProcessorBridge::getKnobLinkRangeMin(int slot, int knob) const { return m_processor.getKnobLinkRangeMin(slot, knob); }
float EditorProcessorBridge::getKnobLinkRangeMax(int slot, int knob) const { return m_processor.getKnobLinkRangeMax(slot, knob); }
void EditorProcessorBridge::setPedalSlot(int slot, DspModuleType type) { m_processor.setPedalSlot(slot, type); }
void EditorProcessorBridge::setPedalGain(int slot, float gain) { m_processor.setPedalGain(slot, gain); }
void EditorProcessorBridge::setManualRouting(const std::vector<uint8_t>& routing) { m_processor.setManualRouting(routing); }
void EditorProcessorBridge::setKnobParameter(int slot, int knob, float start, float value) { m_processor.setKnobParameter(slot, knob, start, value); }
void EditorProcessorBridge::setInputGain(float gain) { m_processor.setInputGain(gain); }
void EditorProcessorBridge::setOutputGain(float gain) { m_processor.setOutputGain(gain); }
void EditorProcessorBridge::setBarCount(int bars) { m_processor.setBarCount(bars); }
void EditorProcessorBridge::setSectionStart(int section) { m_processor.setSectionStart(section); }
void EditorProcessorBridge::setManualMode(bool manual) { m_processor.setManualMode(manual); }
void EditorProcessorBridge::setAutomationValue(float value) { m_processor.setAutomationValue(value); }
void EditorProcessorBridge::notifyPenDown() { m_processor.notifyPenDown(); }
void EditorProcessorBridge::notifyPenUp() { m_processor.notifyPenUp(); }
void EditorProcessorBridge::submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data) { m_processor.submitCanvasSnapshot(data); }
void EditorProcessorBridge::submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data, const DirtyRowMask& dirtyRows) { m_processor.submitCanvasSnapshot(data, dirtyRows); }
void EditorProcessorBridge::scheduleReset() { m_processor.scheduleReset(); }
void EditorProcessorBridge::clearParameterOffsets() { m_processor.clearParamOffsets(); }
void EditorProcessorBridge::resetParameterDefaults() { m_processor.resetParamDefaults(); }
void EditorProcessorBridge::storeUndoData(std::vector<uint8_t> data) { m_processor.storeUndoData(std::move(data)); }
const std::vector<uint8_t>& EditorProcessorBridge::getUndoData() const { return m_processor.getUndoData(); }
void EditorProcessorBridge::storeParameterValue(int slot, int knob, float value) { m_processor.storeParameterValue(slot, knob, value); }
float EditorProcessorBridge::getPlayHeadBpm() const { return m_processor.getPlayHeadBpm(); }
double EditorProcessorBridge::getPlayHeadPpq() const { return m_processor.getPlayHeadPpq(); }
bool EditorProcessorBridge::isPlayHeadPlaying() const { return m_processor.isPlayHeadPlaying(); }
void EditorProcessorBridge::getProjectInformation(juce::MemoryBlock& data) const { m_processor.getStateInformation(data); }
void EditorProcessorBridge::getPresetInformation(juce::MemoryBlock& data) const { m_processor.getPresetInformation(data); }
bool EditorProcessorBridge::setPresetInformation(const void* data, int sizeInBytes) { return m_processor.setPresetInformation(data, sizeInBytes); }
EditorSessionState EditorProcessorBridge::getEditorSessionState() const { return m_processor.getEditorSessionState(); }
void EditorProcessorBridge::setEditorSessionState(const EditorSessionState& state) { m_processor.setEditorSessionState(state); }
void EditorProcessorBridge::drainReleaseQueue() { m_processor.drainReleaseQueue(); }
uint32_t EditorProcessorBridge::getReleaseQueueDroppedCount() const { return m_processor.getReleaseQueueDroppedCount(); }
void EditorProcessorBridge::tryApplyDeferredConfig() { m_processor.tryApplyDeferredConfig(); }
bool EditorProcessorBridge::consumeUINotification() { return m_processor.consumeUINotification(); }
bool EditorProcessorBridge::consumeCompiledResultIfAvailable() { return m_processor.consumeCompiledResultIfAvailable(); }
uint32_t EditorProcessorBridge::getConfigRevision() const { return m_processor.getConfigRevision(); }
const ConfigSyncData& EditorProcessorBridge::getLastConfigSync() const { return m_processor.getLastConfigSync(); }
const std::vector<uint8_t>& EditorProcessorBridge::getManualRouting() const { return m_processor.getManualRouting(); }
bool EditorProcessorBridge::isManualMode() const { return m_processor.isManualMode(); }
bool EditorProcessorBridge::isParamOverridden(int slot, int knob) const { return m_processor.isParamOverridden(slot, knob); }
float EditorProcessorBridge::getKnobDisplayValue(int slot, int knob, float compiled) const { return m_processor.getKnobDisplayValue(slot, knob, compiled); }
std::array<float, TotalKnobs> EditorProcessorBridge::getKnobValues() const { return m_processor.getKnobValues(); }
bool EditorProcessorBridge::isKnobLinked(int slot, int knob) const { return m_processor.isKnobLinked(slot, knob); }
float EditorProcessorBridge::getKnobLinkStrength(int slot, int knob) const { return m_processor.getKnobLinkStrength(slot, knob); }
DspModuleType EditorProcessorBridge::getPedalSlot(int slot) const { return m_processor.getPedalSlot(slot); }
float EditorProcessorBridge::getPedalPeak(int slot) const { return m_processor.getPedalPeak(slot); }
const std::array<uint8_t, TotalCells>& EditorProcessorBridge::getGridData() const { return m_processor.getGridData(); }
int EditorProcessorBridge::getBarCount() const { return m_processor.getBarCount(); }
int EditorProcessorBridge::getSectionStart() const { return m_processor.getSectionStart(); }
void EditorProcessorBridge::resetPedalPeaks() { m_processor.resetPedalPeaks(); }
void EditorProcessorBridge::setManualEnvelopeSlice(int slice, float value) { static_cast<IConfigConsumer&>(m_processor).setManualEnvelopeSlice(slice, value); }
float EditorProcessorBridge::getManualEnvelopeSlice(int slice) const { return static_cast<const IConfigConsumer&>(m_processor).getManualEnvelopeSlice(slice); }
AutomationEnvelope EditorProcessorBridge::getManualEnvelope() const { return static_cast<const IConfigConsumer&>(m_processor).getManualEnvelope(); }
bool EditorProcessorBridge::hasManualEnvelope() const { return static_cast<const IConfigConsumer&>(m_processor).hasManualEnvelope(); }
void EditorProcessorBridge::setLinkRangeEditEnabled(bool enabled)
{
    auto s = m_processor.getEditorSessionState();
    s.linkRangeEditEnabled = enabled;
    m_processor.setEditorSessionState(s);
}
