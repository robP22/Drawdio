#pragma once

#include <cstdint>
#include <vector>

#include "Core/DspModuleType.h"

class IPedalComponentModel
{
public:
    virtual ~IPedalComponentModel() = default;

    virtual void setPedalSlot(int slot, DspModuleType type) = 0;
    virtual DspModuleType getPedalSlot(int slot) const = 0;
    virtual void setKnobParameter(int slot, int knob, float dragStartValue, float newValue) = 0;
    virtual bool isKnobLinked(int slot, int knob) const = 0;
    virtual void setKnobLink(int slot, int knob, bool linked) = 0;
};

class IPedalboardModel : public IPedalComponentModel
{
public:
    virtual bool isManualMode() const = 0;
    virtual void setManualRouting(const std::vector<uint8_t>& routing) = 0;
};

class IMixerStripModel
{
public:
    virtual ~IMixerStripModel() = default;

    virtual DspModuleType getPedalSlot(int slot) const = 0;
    virtual float getPedalPeak(int slot) const = 0;
    virtual float getPedalGain(int slot) const = 0;
    virtual void setPedalGain(int slot, float gain) = 0;
};

class IBottomBarModel : public IMixerStripModel
{
public:
    virtual void setBarCount(int bars) = 0;
    virtual int getBarCount() const = 0;
    virtual void setSectionStart(int sectionStart) = 0;
    virtual int getSectionStart() const = 0;
    virtual void setManualMode(bool manual) = 0;
    virtual bool isManualMode() const = 0;
    virtual float getInputGain() const = 0;
    virtual void setInputGain(float gain) = 0;
    virtual float getOutputGain() const = 0;
    virtual void setOutputGain(float gain) = 0;
};
