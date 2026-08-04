#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include "Core/DrawdioConstants.h"
#include "Core/CompiledPedalConfig.h"
#include "Core/DspModuleType.h"

struct ConfigSyncData
{
    std::vector<ParameterDescriptor> parameters;
    std::vector<uint8_t> routingSlotOrder;
};

class IConfigConsumer
{
public:
    virtual ~IConfigConsumer() = default;

    virtual void drainReleaseQueue() = 0;
    virtual void tryApplyDeferredConfig() = 0;
    virtual bool consumeUINotification() = 0;
    virtual bool consumeCompiledResultIfAvailable() = 0;
    virtual uint32_t getConfigRevision() const = 0;
    virtual const ConfigSyncData& getLastConfigSync() const = 0;
    virtual const PedalAssetPayload* getCurrentConfig() const = 0;
    virtual const std::vector<uint8_t>& getManualRouting() const = 0;
    virtual bool isManualMode() const = 0;
    virtual bool isParamOverridden(int slot, int knob) const = 0;
    virtual float getKnobDisplayValue(int slot, int knob, float compiled) const = 0;
    virtual void storeParameterValue(int slot, int knob, float value) = 0;
    virtual std::array<float, PedalSlotCount * 4> getKnobValues() const = 0;
    virtual bool isKnobLinked(int slot, int knob) const = 0;
    virtual float getKnobLinkStrength(int slot, int knob) const = 0;
    virtual DspModuleType getPedalSlot(int slot) const = 0;
    virtual float getPlayHeadBpm() const = 0;
    virtual double getPlayHeadPpq() const = 0;
    virtual bool isPlayHeadPlaying() const = 0;
    virtual void resetPedalPeaks() = 0;
    virtual void setAutomationValue(float val) = 0;
    virtual void storeUndoData(std::vector<uint8_t> data) = 0;
    virtual const std::array<uint8_t, TotalCells>& getGridData() const = 0;
    virtual const std::vector<uint8_t>& getUndoData() const = 0;
    virtual int getBarCount() const = 0;
    virtual int getSectionStart() const = 0;
};
