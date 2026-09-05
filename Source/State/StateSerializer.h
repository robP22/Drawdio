#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "State/ProjectState.h"

class StateSerializer
{
public:
    enum class DocumentType : uint8_t
    {
        Preset = 1,
        Project = 2
    };

    static constexpr int SchemaVersion = 4;

    static bool serializePreset(const PresetState& state, juce::MemoryBlock& outBlob);
    static bool serializeProject(const ProjectState& state, juce::MemoryBlock& outBlob);
    static bool deserializePreset(const void* data, size_t sizeInBytes, PresetState& outState);
    static bool deserializeProject(const void* data, size_t sizeInBytes, ProjectState& outState);
};
