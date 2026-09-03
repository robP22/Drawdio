#pragma once

#include <cstdint>

namespace ParamToken
{
    constexpr uint16_t Knob0 = 0;
    constexpr uint16_t Knob1 = 1;
    constexpr uint16_t Knob2 = 2;
    constexpr uint16_t Knob3 = 3;
}

struct ParameterDescriptor
{
    uint16_t parameterToken;
    float minValue;
    float maxValue;
    float defaultValue;
    float currentValue;
    bool isManualOverride = false;
    uint8_t targetDspNodeRegister;
    uint8_t physicalSlot = 0;
};
