#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "State/StateSerializer.h"

TEST_CASE("Preset state round-trips through the ValueTree serializer", "[state]")
{
    PresetState input;
    input.gridData[17] = 3;
    input.pedalSlots[1] = DspModuleType::TREMOLO;
    input.manualRouting = { 4, 1, 0, 0, 0, 0 };
    input.manualRoutingSize = 3;
    input.knobValues[5] = 0.73f;
    input.overrideMask = 1u << 5;
    input.barCount = 4;
    input.sectionStartBar = 2;
    input.manualMode = 1;
    input.inputGain = 0.8f;
    input.outputGain = 1.2f;
    input.pedalGains[1] = 0.65f;
    input.linkFlags = 1u << 5;

    juce::MemoryBlock blob;
    REQUIRE(StateSerializer::serializePreset(input, blob));

    PresetState output;
    REQUIRE(StateSerializer::deserializePreset(blob.getData(), blob.getSize(), output));
    REQUIRE(output.gridData == input.gridData);
    REQUIRE(output.pedalSlots == input.pedalSlots);
    REQUIRE(output.manualRoutingSize == input.manualRoutingSize);
    REQUIRE(std::equal(output.manualRouting.begin(),
                       output.manualRouting.begin() + output.manualRoutingSize,
                       input.manualRouting.begin()));
    REQUIRE(output.knobValues == input.knobValues);
    REQUIRE(output.overrideMask == input.overrideMask);
    REQUIRE(output.barCount == input.barCount);
    REQUIRE(output.sectionStartBar == input.sectionStartBar);
    REQUIRE(output.manualMode == input.manualMode);
    REQUIRE(output.inputGain == input.inputGain);
    REQUIRE(output.outputGain == input.outputGain);
    REQUIRE(output.pedalGains == input.pedalGains);
    REQUIRE(output.linkFlags == input.linkFlags);
}

TEST_CASE("Project state preserves session data separately from preset data", "[state]")
{
    ProjectState input;
    input.preset.pedalSlots[0] = DspModuleType::WAVESHAPER;
    input.session.selectedColour = 12;
    input.session.selectedTool = 2;
    input.session.selectedPedal = 4;

    juce::MemoryBlock blob;
    REQUIRE(StateSerializer::serializeProject(input, blob));

    ProjectState output;
    REQUIRE(StateSerializer::deserializeProject(blob.getData(), blob.getSize(), output));
    REQUIRE(output.preset.pedalSlots == input.preset.pedalSlots);
    REQUIRE(output.session.selectedColour == input.session.selectedColour);
    REQUIRE(output.session.selectedTool == input.session.selectedTool);
    REQUIRE(output.session.selectedPedal == input.session.selectedPedal);
}

TEST_CASE("Preset and project documents cannot be interchanged", "[state]")
{
    PresetState preset;
    ProjectState project;
    juce::MemoryBlock presetBlob;
    juce::MemoryBlock projectBlob;
    REQUIRE(StateSerializer::serializePreset(preset, presetBlob));
    REQUIRE(StateSerializer::serializeProject(project, projectBlob));

    REQUIRE_FALSE(StateSerializer::deserializeProject(presetBlob.getData(), presetBlob.getSize(), project));
    REQUIRE_FALSE(StateSerializer::deserializePreset(projectBlob.getData(), projectBlob.getSize(), preset));
}

TEST_CASE("State deserialization rejects invalid input bounds", "[state]")
{
    PresetState state;
    REQUIRE_FALSE(StateSerializer::deserializePreset(nullptr, 0, state));
}

TEST_CASE("State deserialization rejects duplicate routing and invalid ranges", "[state]")
{
    PresetState state;
    state.manualRouting = { 1, 1, 0, 0, 0, 0 };
    state.manualRoutingSize = 2;

    juce::MemoryBlock blob;
    REQUIRE(StateSerializer::serializePreset(state, blob));

    PresetState output;
    REQUIRE_FALSE(StateSerializer::deserializePreset(blob.getData(), blob.getSize(), output));

    state.manualRoutingSize = 0;
    state.barCount = 0;
    REQUIRE(StateSerializer::serializePreset(state, blob));
    REQUIRE_FALSE(StateSerializer::deserializePreset(blob.getData(), blob.getSize(), output));
}
