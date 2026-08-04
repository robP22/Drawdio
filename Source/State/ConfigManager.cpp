#include "State/ConfigManager.h"
#include "Dsp/DspEffectFactory.h"

ConfigManager::ConfigManager(UnifiedPedalProcessor& dsp)
    : m_dsp(dsp)
{
    m_pedalSlots.fill(DspModuleType::BYPASS);
    m_gridData.fill(0);
}

ConfigManager::~ConfigManager()
{
    m_releaseQueue.drain();

    auto* current = m_currentConfig.load(std::memory_order_relaxed);
    if (current) delete current;

    auto* next = m_nextConfig.load(std::memory_order_relaxed);
    if (next) delete next;

    auto* deferred = m_deferredConfig.load(std::memory_order_relaxed);
    if (deferred) delete deferred;
}

void ConfigManager::setPedalSlot(int slot, DspModuleType type)
{
    if (slot < 0 || slot >= static_cast<int>(m_pedalSlots.size()))
        return;

    DspModuleType oldType = m_pedalSlots[static_cast<size_t>(slot)];
    m_pedalSlots[static_cast<size_t>(slot)] = type;

    if (type == DspModuleType::BYPASS)
    {
        std::vector<uint8_t> filtered;
        for (auto s : m_manualRouting)
            if (s != static_cast<uint8_t>(slot))
                filtered.push_back(s);
        m_manualRouting = filtered;
    }
    else if (oldType == DspModuleType::BYPASS && !m_manualRouting.empty())
    {
        if (std::find(m_manualRouting.begin(), m_manualRouting.end(),
                      static_cast<uint8_t>(slot)) == m_manualRouting.end())
            m_manualRouting.push_back(static_cast<uint8_t>(slot));
    }

    m_dsp.invalidateParamCacheForSlot(slot);
    syncCompilerConfig();
    triggerUINotification();
}

DspModuleType ConfigManager::getPedalSlot(int slot) const
{
    if (slot >= 0 && slot < static_cast<int>(m_pedalSlots.size()))
        return m_pedalSlots[static_cast<size_t>(slot)];
    return DspModuleType::BYPASS;
}

void ConfigManager::setGridData(const std::array<uint8_t, TotalCells>& data)
{
    m_gridData = data;
}

void ConfigManager::setManualRouting(const std::vector<uint8_t>& routing)
{
    m_manualRouting = routing;
    syncCompilerConfig();
}

void ConfigManager::setManualMode(bool m)
{
    m_manualMode = m;
    if (!m)
    {
        m_manualRouting.clear();
        syncCompilerConfig();
    }
}

void ConfigManager::prepare(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    std::vector<DspModuleType> slots(m_pedalSlots.begin(), m_pedalSlots.end());
    m_compilerThread.setPedalSlots(slots);
    m_compilerThread.start(m_messageQueue, m_penDebouncer);
}

void ConfigManager::releaseResources()
{
    m_compilerThread.stop();
}

void ConfigManager::syncCompilerConfig()
{
    std::vector<DspModuleType> slots(m_pedalSlots.begin(), m_pedalSlots.end());
    m_compilerThread.setPedalSlots(slots);
    m_compilerThread.setManualRouting(m_manualRouting);
    m_compilerThread.setExistingParameters(getCurrentParams());
    m_messageQueue.pushSnapshot(m_gridData.data());
    m_compilerThread.notify();
}

void ConfigManager::prebuildEffects(const PedalAssetPayload* config, bool& deferred)
{
    deferred = false;
    if (!config) return;

    if (m_nextConfig.load(std::memory_order_acquire) != nullptr)
    {
        deferred = true;
        return;
    }

    double sr = m_dsp.getSampleRate();
    int maxCh = m_dsp.getMaxChannels();

    for (size_t i = 0; i < m_chainEffects.size(); ++i)
    {
        DspModuleType neededType = (i < config->activeRoutingChain.size())
            ? config->activeRoutingChain[i]
            : DspModuleType::BYPASS;

        if (neededType != DspModuleType::BYPASS)
        {
            m_pendingEffects[i] = createDspEffect(neededType);
            if (m_pendingEffects[i])
                m_pendingEffects[i]->prepare(sr, maxCh);
        }
        else
        {
            m_pendingEffects[i].reset();
        }
    }
}

void ConfigManager::loadPedalConfiguration(const PedalAssetPayload* config)
{
    if (!config) return;

    for (auto& row : m_paramPtrs)
        row.fill(nullptr);
    for (auto& p : config->parameters)
    {
        auto reg = p.targetDspNodeRegister;
        if (reg < PedalSlotCount && p.parameterToken < 4)
            m_paramPtrs[reg][p.parameterToken] = &p.currentValue;
    }

    const auto* current = m_currentConfig.load(std::memory_order_acquire);
    {
        auto* stale = m_deferredConfig.exchange(nullptr, std::memory_order_acq_rel);
        if (stale)
            m_releaseQueue.push(stale);
    }

    if (current && !current->activeRoutingChain.empty())
    {
        bool prebuildDeferred = false;
        prebuildEffects(config, prebuildDeferred);
        if (prebuildDeferred)
        {
            m_deferredConfig.store(config, std::memory_order_release);
            return;
        }
        m_nextConfig.store(config, std::memory_order_release);
        m_dsp.crossfadeReset();
    }
    else
    {
        bool unused = false;
        prebuildEffects(config, unused);
        for (size_t i = 0; i < m_chainEffects.size(); ++i)
            if (m_pendingEffects[i])
                std::swap(m_chainEffects[i], m_pendingEffects[i]);

        m_dsp.reset(m_chainEffects);

        const auto* oldCurrent = m_currentConfig.exchange(config, std::memory_order_acq_rel);
        if (oldCurrent)
            m_releaseQueue.push(oldCurrent);
    }
}

bool ConfigManager::consumeCompiledResultIfAvailable()
{
    if (!m_compilerThread.hasCompiledResult())
        return false;

    auto* payloadPtr = m_compilerThread.getCompiledPayloadPtr();
    if (!payloadPtr)
        return false;

    m_lastConfigSync.parameters = payloadPtr->parameters;
    m_lastConfigSync.routingSlotOrder = payloadPtr->routingSlotOrder;
    loadPedalConfiguration(payloadPtr);
    m_configRevision.fetch_add(1, std::memory_order_acq_rel);
    triggerUINotification();
    return true;
}

bool ConfigManager::consumeUINotification()
{
    bool expected = true;
    return m_uiNeedsUpdate.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
}

void ConfigManager::triggerUINotification()
{
    m_uiNeedsUpdate.store(true, std::memory_order_release);
}

void ConfigManager::tryApplyDeferredConfig()
{
    auto* deferred = m_deferredConfig.exchange(nullptr, std::memory_order_acq_rel);
    if (deferred)
    {
        if (m_nextConfig.load(std::memory_order_acquire) == nullptr)
            loadPedalConfiguration(deferred);
        else
            m_deferredConfig.store(deferred, std::memory_order_release);
    }
}

std::vector<ParameterDescriptor> ConfigManager::getCurrentParams() const
{
    const auto* current = m_currentConfig.load(std::memory_order_acquire);
    if (current)
        return current->parameters;
    return {};
}

void ConfigManager::submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data)
{
    m_messageQueue.pushSnapshot(data.data());
    m_gridData = data;
    m_compilerThread.notify();
}

void ConfigManager::getStateInformation(juce::MemoryBlock& destData)
{
    auto snap = m_dsp.getSnapshot();
    auto mask = m_dsp.getParamOverrideMask();
    auto state = StateSerializer::createState(m_gridData, m_pedalSlots, m_manualRouting, snap.values, mask,
                                              static_cast<uint8_t>(m_barCount),
                                              static_cast<uint8_t>(m_sectionStartBar),
                                              static_cast<uint8_t>(m_manualMode ? 1 : 0));
    StateSerializer::serialize(state, destData);
}

void ConfigManager::setStateInformation(const void* data, int sizeInBytes)
{
    StateSerializer::SerializedState state;
    if (!StateSerializer::deserialize(static_cast<const uint8_t*>(data),
                                      static_cast<size_t>(sizeInBytes), state))
        return;

    m_gridData = state.gridData;
    m_pedalSlots = state.pedalSlots;
    m_manualRouting = state.manualRouting;
    restoreKnobValuesFromState(state);
    m_barCount = state.barCount;
    m_sectionStartBar = state.sectionStartBar;
    m_manualMode = (state.manualMode != 0);
    m_dsp.scheduleReset();
    syncCompilerConfig();
}

juce::MemoryBlock ConfigManager::createPresetState()
{
    auto snap = m_dsp.getSnapshot();
    auto mask = m_dsp.getParamOverrideMask();
    auto state = StateSerializer::createState(m_gridData, m_pedalSlots, m_manualRouting, snap.values, mask,
                                              static_cast<uint8_t>(m_barCount),
                                              static_cast<uint8_t>(m_sectionStartBar),
                                              static_cast<uint8_t>(m_manualMode ? 1 : 0));
    juce::MemoryBlock result;
    StateSerializer::serialize(state, result);
    return result;
}

bool ConfigManager::applyPresetState(const void* data, int sizeInBytes)
{
    StateSerializer::SerializedState state;
    if (!StateSerializer::deserialize(static_cast<const uint8_t*>(data),
                                     static_cast<size_t>(sizeInBytes), state))
        return false;

    m_gridData = state.gridData;
    m_pedalSlots = state.pedalSlots;
    m_manualRouting = state.manualRouting;
    restoreKnobValuesFromState(state);
    m_dsp.scheduleReset();
    syncCompilerConfig();
    return true;
}

void ConfigManager::restoreKnobValuesFromState(const StateSerializer::SerializedState& state)
{
    for (int s = 0; s < PedalSlotCount; ++s)
        for (int k = 0; k < 4; ++k)
        {
            size_t idx = static_cast<size_t>(s * 4 + k);
            if (state.overrideMask & (1u << idx))
                m_dsp.updateParameter(s, k, state.knobValues[idx]);
            else
                m_dsp.storeParameterValue(s, k, state.knobValues[idx]);
        }
}

void ConfigManager::setPlayHeadPosition(float bpm, double ppq, bool playing)
{
    m_playHeadBpm.store(bpm, std::memory_order_release);
    m_playHeadPpq.store(ppq, std::memory_order_release);
    m_playHeadPlaying.store(playing, std::memory_order_release);
}
