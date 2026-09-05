#include "State/ConfigManager.h"
#include "Dsp/DspEffectFactory.h"
#include "State/PedalDefinition.h"

ConfigManager::ConfigManager(UnifiedPedalProcessor& dsp)
    : m_dsp(dsp)
{
    m_pedalSlots.fill(DspModuleType::BYPASS);
    m_gridData.fill(0);
    m_compilerThread.setResultAvailableCallback([this]() { triggerUINotification(); });
}

ConfigManager::~ConfigManager()
{
    m_compilerThread.setResultAvailableCallback({});
    removeAllChangeListeners();
    m_compilerThread.stop();

    auto scheduleDelete = [](const PedalAssetPayload* ptr)
    {
        if (!ptr)
            return;
        if (juce::MessageManager::existsAndIsCurrentThread()
            || juce::MessageManager::getInstanceWithoutCreating() == nullptr)
            delete ptr;
        else
            juce::MessageManager::callAsync([ptr]() { delete ptr; });
    };

    scheduleDelete(m_currentConfig.exchange(nullptr, std::memory_order_acq_rel));
    scheduleDelete(m_nextConfig.exchange(nullptr, std::memory_order_acq_rel));
    scheduleDelete(m_deferredConfig.exchange(nullptr, std::memory_order_acq_rel));
    m_releaseQueue.drainAsync();
}

void ConfigManager::setPedalSlot(int slot, DspModuleType type)
{
    if (slot < 0 || slot >= static_cast<int>(m_pedalSlots.size()))
        return;

    DspModuleType oldType = m_pedalSlots[static_cast<size_t>(slot)];
    m_pedalSlots[static_cast<size_t>(slot)] = type;

    if (type != oldType)
    {
        std::vector<uint8_t> filtered;
        for (auto s : m_manualRouting)
            if (s != static_cast<uint8_t>(slot))
                filtered.push_back(s);
        m_manualRouting = filtered;

        auto& ps = m_dsp.pedalState();
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            ps.setKnobLink(slot, k, false);
            ps.setKnobLinkRange(slot, k, 0.0f, 1.0f);
        }
    }
    else if (oldType == DspModuleType::BYPASS && !m_manualRouting.empty())
    {
        if (std::find(m_manualRouting.begin(), m_manualRouting.end(),
                      static_cast<uint8_t>(slot)) == m_manualRouting.end())
            m_manualRouting.push_back(static_cast<uint8_t>(slot));
    }

    m_dsp.invalidateParamCacheForSlot(slot);
    {
        const auto& def = PedalDefinitions::get(type);
        for (int k = 0; k < KnobsPerPedal; ++k)
            m_dsp.storeParameterValue(slot, k,
                def.parameters[static_cast<size_t>(k)].param.defaultValue);
    }
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
    triggerUINotification();
}

void ConfigManager::setManualMode(bool m)
{
    m_manualMode = m;
    if (m)
    {
        seedCacheFromCurrentConfig();
        PresetState snapshot;
        capturePresetState(snapshot);
        if (snapshot.hasManualEnvelope)
        {
            AutomationEnvelope env;
            for (int i = 0; i < EnvelopeSliceCount; ++i)
                env.addPoint(static_cast<float>(i) / static_cast<float>(EnvelopeSliceCount - 1),
                             snapshot.manualEnvelope[static_cast<size_t>(i)]);
            m_manualEnvelope = std::move(env);
            m_hasManualEnvelope = true;
            m_sessionState.isManualEnvelopeOverridden = true;
        }
    }
    else
    {
        m_manualRouting.clear();
        syncCompilerConfig();
    }
}

void ConfigManager::setManualEnvelopeSlice(int slice, float value)
{
    if (slice < 0 || slice >= EnvelopeSliceCount)
        return;
    value = std::clamp(value, 0.0f, 1.0f);
    if (m_manualEnvelope.empty())
    {
        for (int i = 0; i < EnvelopeSliceCount; ++i)
            m_manualEnvelope.addPoint(static_cast<float>(i) / static_cast<float>(EnvelopeSliceCount - 1), 0.5f);
    }
    float t = static_cast<float>(slice) / static_cast<float>(EnvelopeSliceCount - 1);
    auto points = m_manualEnvelope.getPoints();
    bool found = false;
    for (auto& p : points)
        if (std::abs(p.time - t) < 1e-6f) { p.value = value; found = true; break; }
    if (!found)
    {
        m_manualEnvelope.addPoint(t, value);
        points = m_manualEnvelope.getPoints();
    }
    if (found)
    {
        AutomationEnvelope rebuilt;
        for (const auto& p : points)
            rebuilt.addPoint(p.time, p.time == t ? value : p.value);
        m_manualEnvelope = std::move(rebuilt);
    }
    m_hasManualEnvelope = true;
    m_sessionState.isManualEnvelopeOverridden = true;
    triggerUINotification();
}

float ConfigManager::getManualEnvelopeSlice(int slice) const
{
    if (slice < 0 || slice >= EnvelopeSliceCount)
        return 0.5f;
    float t = static_cast<float>(slice) / static_cast<float>(EnvelopeSliceCount - 1);
    const auto points = m_manualEnvelope.getPoints();
    for (const auto& p : points)
        if (std::abs(p.time - t) < 1e-6f)
            return p.value;
    return m_manualEnvelope.empty() ? 0.5f : m_manualEnvelope.sample(t);
}

void ConfigManager::seedCacheFromCurrentConfig()
{
    const auto* current = m_currentConfig.load(std::memory_order_acquire);
    if (!current)
        return;
    const auto& slotOrder = current->routingSlotOrder;
    for (const auto& p : current->parameters)
    {
        auto chainPos = p.targetDspNodeRegister;
        if (chainPos >= slotOrder.size())
            continue;
        int slot = slotOrder[chainPos];
        if (m_dsp.isParamOverridden(slot, p.parameterToken))
            continue;
        m_dsp.storeParameterValue(slot, p.parameterToken, p.currentValue);
    }
}

void ConfigManager::resetParamDefaults()
{
    for (int s = 0; s < PedalSlotCount; ++s)
    {
        const auto& def = PedalDefinitions::get(m_pedalSlots[static_cast<size_t>(s)]);
        for (int k = 0; k < KnobsPerPedal; ++k)
            m_dsp.storeParameterValue(s, k,
                def.parameters[static_cast<size_t>(k)].param.defaultValue);
    }
}

void ConfigManager::prepare(double sampleRate, int samplesPerBlock)
{
    const int maxChannels = m_dsp.getMaxChannels();
    const bool preparationChanged = std::abs(sampleRate - m_sampleRate) > 0.5
        || samplesPerBlock != m_preparedBlockSize
        || maxChannels != m_preparedChannels;

    if (preparationChanged)
    {
        m_sampleRate = sampleRate;
        m_preparedBlockSize = samplesPerBlock;
        m_preparedChannels = maxChannels;
        rePrepareEffects();
    }

    std::vector<DspModuleType> slots(m_pedalSlots.begin(), m_pedalSlots.end());
    m_compilerThread.start(m_messageQueue, m_penDebouncer);
    DirtyRowMask allRows;
    allRows.fill(~uint64_t{ 0 });
    if (!m_messageQueue.pushSnapshot(m_gridData.data(), allRows, ++m_canvasRevision,
                                     slots, m_manualRouting, getCurrentParams()))
        m_compileRetryPending = true;
    m_compilerThread.notify();
}

void ConfigManager::rePrepareEffects()
{
    double sr = m_dsp.getSampleRate();
    int maxCh = m_dsp.getMaxChannels();

    auto reprepare = [sr, maxCh](const PedalAssetPayload* config) {
        if (!config) return;
        for (auto& effect : config->effects)
            if (effect)
                effect->prepare(sr, maxCh);
    };

    reprepare(m_currentConfig.load(std::memory_order_acquire));
    reprepare(m_nextConfig.load(std::memory_order_acquire));
    reprepare(m_deferredConfig.load(std::memory_order_acquire));
}

void ConfigManager::releaseResources()
{
    // The compiler thread is UI-driven and lives for the instance lifetime; it
    // is stopped only in the destructor. Stopping it here would freeze the
    // compile pipeline while a host keeps the plugin loaded but disabled
    // (e.g. FL's mixer enable/disable toggle).
}

void ConfigManager::syncCompilerConfig()
{
    std::vector<DspModuleType> slots(m_pedalSlots.begin(), m_pedalSlots.end());
    DirtyRowMask allRows;
    allRows.fill(~uint64_t{ 0 });
    if (!m_messageQueue.pushSnapshot(m_gridData.data(), allRows, ++m_canvasRevision,
                                     slots, m_manualRouting, getCurrentParams()))
        m_compileRetryPending = true;
    m_compilerThread.notify();
}

void ConfigManager::prebuildEffects(PedalAssetPayload* config, bool& deferred)
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

    for (size_t i = 0; i < config->effects.size(); ++i)
    {
        DspModuleType neededType = (i < config->activeRoutingChain.size())
            ? config->activeRoutingChain[i]
            : DspModuleType::BYPASS;

        if (neededType != DspModuleType::BYPASS)
        {
            config->effects[i] = createDspEffect(neededType);
            if (config->effects[i])
                config->effects[i]->prepare(sr, maxCh);
        }
        else
        {
            config->effects[i].reset();
        }
    }
}

void ConfigManager::loadPedalConfiguration(PedalAssetPayload* config)
{
    if (!config) return;

    config->manualParams = m_manualMode;

    for (auto& row : config->paramPtrs)
        row.fill(nullptr);
    for (auto& row : config->snapSteps)
        row.fill(0);
    for (size_t i = 0; i < config->routingSlotOrder.size() && i < config->activeRoutingChain.size(); ++i)
    {
        const auto type = config->activeRoutingChain[i];
        for (int k = 0; k < KnobsPerPedal; ++k)
            config->snapSteps[i][static_cast<size_t>(k)] =
                static_cast<uint8_t>(PedalDefinitions::snapSteps(type, k));
    }
    for (auto& p : config->parameters)
    {
        auto reg = p.targetDspNodeRegister;
        if (reg < PedalSlotCount && p.parameterToken < KnobsPerPedal)
            config->paramPtrs[reg][p.parameterToken] = &p.currentValue;
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
            triggerUINotification();
            return;
        }
        updateCompiledParameterBank(config);
        m_nextConfig.store(config, std::memory_order_release);
        m_dsp.crossfadeReset();
        syncLastConfig(config);
    }
    else
    {
        bool unused = false;
        prebuildEffects(config, unused);
        updateCompiledParameterBank(config);
        m_dsp.reset(*config);

        const auto* oldCurrent = m_currentConfig.exchange(config, std::memory_order_acq_rel);
        if (oldCurrent)
            m_releaseQueue.push(oldCurrent);
        syncLastConfig(config);
    }
}

void ConfigManager::syncLastConfig(const PedalAssetPayload* config)
{
    m_lastConfigSync.parameters = config->parameters;
    m_lastConfigSync.routingSlotOrder = config->routingSlotOrder;
    m_configRevision.fetch_add(1, std::memory_order_acq_rel);
    triggerUINotification();
}

bool ConfigManager::consumeCompiledResultIfAvailable()
{
    retryPendingCompile();
    if (!m_compilerThread.hasCompiledResult())
        return false;

    auto* payloadPtr = m_compilerThread.getCompiledPayloadPtr();
    if (!payloadPtr)
        return false;

    if (payloadPtr->sourceRevision != 0 && payloadPtr->sourceRevision < m_canvasRevision)
    {
        delete payloadPtr;
        return false;
    }

    const auto* current = m_currentConfig.load(std::memory_order_acquire);
    if (current && m_nextConfig.load(std::memory_order_acquire) == nullptr
        && current->manualParams == m_manualMode
        && current->activeRoutingChain == payloadPtr->activeRoutingChain
        && current->routingSlotOrder == payloadPtr->routingSlotOrder)
    {
        publishCompiledParameters(payloadPtr);
        delete payloadPtr;
        return true;
    }

    loadPedalConfiguration(payloadPtr);
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
    sendChangeMessage();
}

void ConfigManager::publishCompiledParameters(const PedalAssetPayload* config)
{
    if (config == nullptr)
        return;

    updateCompiledParameterBank(config);
    m_lastConfigSync.parameters = config->parameters;
    m_lastConfigSync.routingSlotOrder = config->routingSlotOrder;
    m_configRevision.fetch_add(1, std::memory_order_acq_rel);
    triggerUINotification();
}

void ConfigManager::updateCompiledParameterBank(const PedalAssetPayload* config)
{
    if (config == nullptr)
        return;

    for (const auto& parameter : config->parameters)
    {
        const auto chainPos = static_cast<size_t>(parameter.targetDspNodeRegister);
        if (chainPos >= config->routingSlotOrder.size() || parameter.parameterToken >= KnobsPerPedal)
            continue;
        m_dsp.setCompiledParameterValue(
            config->routingSlotOrder[chainPos],
            static_cast<int>(parameter.parameterToken),
            parameter.currentValue);
    }
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
    if (!current)
        return {};
    std::vector<ParameterDescriptor> filtered;
    filtered.reserve(current->parameters.size());
    for (const auto& p : current->parameters)
    {
        if (p.isManualOverride
            && !m_dsp.pedalState().isKnobLinked(p.physicalSlot, static_cast<int>(p.parameterToken)))
            continue;
        filtered.push_back(p);
    }
    return filtered;
}

void ConfigManager::submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data)
{
    m_gridData = data;
    DirtyRowMask allRows;
    allRows.fill(~uint64_t{ 0 });
    if (!m_messageQueue.pushSnapshot(data.data(), allRows, ++m_canvasRevision,
                                     std::vector<DspModuleType>(m_pedalSlots.begin(), m_pedalSlots.end()),
                                     m_manualRouting, getCurrentParams()))
        m_compileRetryPending = true;
    m_compilerThread.notify();
}

void ConfigManager::retryPendingCompile()
{
    if (!m_compileRetryPending)
        return;

    DirtyRowMask allRows;
    allRows.fill(~uint64_t{ 0 });
    const std::vector<DspModuleType> slots(m_pedalSlots.begin(), m_pedalSlots.end());
    if (m_messageQueue.pushSnapshot(m_gridData.data(), allRows, m_canvasRevision,
                                     slots, m_manualRouting, getCurrentParams()))
    {
        m_compileRetryPending = false;
        m_compilerThread.notify();
    }
}

void ConfigManager::submitCanvasSnapshot(const std::array<uint8_t, TotalCells>& data,
                                         const DirtyRowMask& dirtyRows)
{
    m_gridData = data;
    if (!m_messageQueue.pushSnapshot(data.data(), dirtyRows, ++m_canvasRevision,
                                     std::vector<DspModuleType>(m_pedalSlots.begin(), m_pedalSlots.end()),
                                     m_manualRouting, getCurrentParams()))
        m_compileRetryPending = true;
    m_compilerThread.notify();
}

void ConfigManager::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = std::make_unique<ProjectState>();
    capturePresetState(state->preset);
    state->session = m_sessionState;
    StateSerializer::serializeProject(*state, destData);
}

void ConfigManager::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    auto state = std::make_unique<ProjectState>();
    if (!StateSerializer::deserializeProject(data, static_cast<size_t>(sizeInBytes), *state))
        return;

    applyPresetState(state->preset);
    setEditorSessionState(state->session);
}

void ConfigManager::getPresetInformation(juce::MemoryBlock& destData) const
{
    StateSerializer::serializePreset(capturePresetState(), destData);
}

bool ConfigManager::setPresetInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return false;

    auto state = std::make_unique<PresetState>();
    if (!StateSerializer::deserializePreset(data, static_cast<size_t>(sizeInBytes), *state))
        return false;

    applyPresetState(*state);
    return true;
}

PresetState ConfigManager::capturePresetState() const
{
    PresetState state;
    capturePresetState(state);
    return state;
}

void ConfigManager::capturePresetState(PresetState& state) const
{
    state = PresetState{};
    state.gridData = m_gridData;
    state.pedalSlots = m_pedalSlots;
    state.manualRoutingSize = static_cast<uint8_t>(std::min(m_manualRouting.size(), state.manualRouting.size()));
    for (int i = 0; i < state.manualRoutingSize; ++i)
        state.manualRouting[static_cast<size_t>(i)] = m_manualRouting[static_cast<size_t>(i)];

    const auto snap = m_dsp.getSnapshot();
    state.knobValues = snap.values;
    state.overrideMask = snap.mask;
    state.barCount = static_cast<uint8_t>(m_barCount);
    state.sectionStartBar = static_cast<uint8_t>(m_sectionStartBar);
    state.manualMode = static_cast<uint8_t>(m_manualMode ? 1 : 0);

    const auto& ps = m_dsp.pedalState();
    state.inputGain = ps.getInputGain();
    state.outputGain = ps.getOutputGain();
    for (int s = 0; s < PedalSlotCount; ++s)
    {
        state.pedalGains[static_cast<size_t>(s)] = ps.getPedalGain(s);
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            if (ps.isKnobLinked(s, k))
                state.linkFlags |= (1u << static_cast<uint32_t>(s * KnobsPerPedal + k));
            const size_t idx = static_cast<size_t>(s * KnobsPerPedal + k);
            state.linkRangeMins[idx] = ps.getKnobLinkRangeMin(s, k);
            state.linkRangeMaxs[idx] = ps.getKnobLinkRangeMax(s, k);
        }
    }
}

void ConfigManager::applyPresetState(const PresetState& state)
{
    m_gridData = state.gridData;
    m_pedalSlots = state.pedalSlots;
    m_manualRouting.clear();
    for (int i = 0; i < state.manualRoutingSize; ++i)
        m_manualRouting.push_back(state.manualRouting[static_cast<size_t>(i)]);
    m_dsp.clearParamOffsets();
    restoreKnobValuesFromState(state);
    m_barCount = state.barCount;
    m_sectionStartBar = state.sectionStartBar;
    m_manualMode = (state.manualMode != 0);
    auto& ps = m_dsp.pedalState();
    ps.setInputGain(state.inputGain);
    ps.setOutputGain(state.outputGain);
    for (int s = 0; s < PedalSlotCount; ++s)
    {
        ps.setPedalGain(s, state.pedalGains[static_cast<size_t>(s)]);
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            const bool linked = (state.linkFlags & (1u << static_cast<uint32_t>(s * KnobsPerPedal + k))) != 0;
            ps.setKnobLink(s, k, linked);
            const size_t idx = static_cast<size_t>(s * KnobsPerPedal + k);
            if (linked)
                ps.setKnobLinkRange(s, k, state.linkRangeMins[idx], state.linkRangeMaxs[idx]);
            else
                ps.setKnobLinkRange(s, k, 0.0f, 1.0f);
        }
    }
    m_dsp.scheduleReset();
    syncCompilerConfig();
    triggerUINotification();
}

void ConfigManager::restoreKnobValuesFromState(const PresetState& state)
{
    const bool forceAll = (state.manualMode != 0);
    for (int s = 0; s < PedalSlotCount; ++s)
        for (int k = 0; k < KnobsPerPedal; ++k)
        {
            size_t idx = static_cast<size_t>(s * KnobsPerPedal + k);
            if (forceAll || (state.overrideMask & (1u << idx)))
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
    m_dsp.setTransport(bpm, ppq, playing);
}
