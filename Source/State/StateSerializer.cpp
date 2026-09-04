#include "StateSerializer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
const juce::Identifier kRoot("DrawdioState");
const juce::Identifier kType("type");
const juce::Identifier kVersion("version");
const juce::Identifier kPreset("preset");
const juce::Identifier kSession("session");
const juce::Identifier kGrid("grid");
const juce::Identifier kPedals("pedals");
const juce::Identifier kRouting("routing");
const juce::Identifier kKnobs("knobs");
const juce::Identifier kOverrideMask("overrideMask");
const juce::Identifier kBarCount("barCount");
const juce::Identifier kSectionStart("sectionStart");
const juce::Identifier kManualMode("manualMode");
const juce::Identifier kInputGain("inputGain");
const juce::Identifier kOutputGain("outputGain");
const juce::Identifier kPedalGains("pedalGains");
const juce::Identifier kLinkFlags("linkFlags");
const juce::Identifier kLinkRangeMins("linkRangeMins");
const juce::Identifier kLinkRangeMaxs("linkRangeMaxs");
const juce::Identifier kSelectedColour("selectedColour");
const juce::Identifier kSelectedTool("selectedTool");
const juce::Identifier kSelectedPedal("selectedPedal");
const juce::Identifier kBrushSizeIndex("brushSizeIndex");
const juce::Identifier kLinkRangeEditEnabled("linkRangeEditEnabled");
const juce::Identifier kManualEnvelope("manualEnvelope");
const juce::Identifier kHasManualEnvelope("hasManualEnvelope");

juce::MemoryBlock makeBytes(const void* data, size_t size)
{
    juce::MemoryBlock result;
    result.append(data, size);
    return result;
}

template <typename T, size_t N>
juce::MemoryBlock makeArrayBytes(const std::array<T, N>& values)
{
    return makeBytes(values.data(), sizeof(T) * N);
}

template <typename T, size_t N>
bool readArrayBytes(const juce::var& value, std::array<T, N>& output)
{
    const auto* block = value.getBinaryData();
    if (block == nullptr || block->getSize() != sizeof(T) * N)
        return false;

    std::memcpy(output.data(), block->getData(), block->getSize());
    return true;
}

template <typename T, size_t N>
bool finiteArray(const std::array<T, N>& values)
{
    for (const auto value : values)
        if (!std::isfinite(value))
            return false;
    return true;
}

juce::ValueTree makePresetTree(const PresetState& state)
{
    juce::ValueTree tree(kPreset);
    tree.setProperty(kGrid, juce::var(makeArrayBytes(state.gridData)), nullptr);

    std::array<uint8_t, PedalSlotCount> pedals{};
    for (int i = 0; i < PedalSlotCount; ++i)
        pedals[static_cast<size_t>(i)] = static_cast<uint8_t>(state.pedalSlots[static_cast<size_t>(i)]);

    tree.setProperty(kPedals, juce::var(makeArrayBytes(pedals)), nullptr);
    tree.setProperty(kRouting, juce::var(makeBytes(state.manualRouting.data(), state.manualRoutingSize)), nullptr);
    tree.setProperty(kKnobs, juce::var(makeArrayBytes(state.knobValues)), nullptr);
    tree.setProperty(kOverrideMask, static_cast<int64_t>(state.overrideMask), nullptr);
    tree.setProperty(kBarCount, static_cast<int>(state.barCount), nullptr);
    tree.setProperty(kSectionStart, static_cast<int>(state.sectionStartBar), nullptr);
    tree.setProperty(kManualMode, static_cast<int>(state.manualMode), nullptr);
    tree.setProperty(kInputGain, state.inputGain, nullptr);
    tree.setProperty(kOutputGain, state.outputGain, nullptr);
    tree.setProperty(kPedalGains, juce::var(makeArrayBytes(state.pedalGains)), nullptr);
    tree.setProperty(kLinkFlags, static_cast<int64_t>(state.linkFlags), nullptr);
    tree.setProperty(kLinkRangeMins, juce::var(makeArrayBytes(state.linkRangeMins)), nullptr);
    tree.setProperty(kLinkRangeMaxs, juce::var(makeArrayBytes(state.linkRangeMaxs)), nullptr);
    tree.setProperty(kHasManualEnvelope, state.hasManualEnvelope ? 1 : 0, nullptr);
    tree.setProperty(kManualEnvelope, juce::var(makeArrayBytes(state.manualEnvelope)), nullptr);
    return tree;
}

bool readPresetTree(const juce::ValueTree& tree, PresetState& state)
{
    if (!tree.isValid()
        || !readArrayBytes(tree[kGrid], state.gridData)
        || !readArrayBytes(tree[kKnobs], state.knobValues)
        || !readArrayBytes(tree[kPedalGains], state.pedalGains)
        || !tree.hasProperty(kRouting)
        || !tree.hasProperty(kOverrideMask)
        || !tree.hasProperty(kBarCount)
        || !tree.hasProperty(kSectionStart)
        || !tree.hasProperty(kManualMode)
        || !tree.hasProperty(kInputGain)
        || !tree.hasProperty(kOutputGain)
        || !tree.hasProperty(kLinkFlags))
        return false;
    state.linkRangeMins.fill(0.0f);
    state.linkRangeMaxs.fill(1.0f);
    state.hasManualEnvelope = false;
    state.manualEnvelope.fill(0.5f);
    if (tree.hasProperty(kLinkRangeMins))
    {
        if (!readArrayBytes(tree[kLinkRangeMins], state.linkRangeMins))
            return false;
    }
    if (tree.hasProperty(kLinkRangeMaxs))
    {
        if (!readArrayBytes(tree[kLinkRangeMaxs], state.linkRangeMaxs))
            return false;
    }
    if (tree.hasProperty(kHasManualEnvelope))
        state.hasManualEnvelope = static_cast<int>(tree[kHasManualEnvelope]) != 0;
    if (tree.hasProperty(kManualEnvelope))
    {
        if (!readArrayBytes(tree[kManualEnvelope], state.manualEnvelope))
            return false;
    }

    std::array<uint8_t, PedalSlotCount> pedals{};
    if (!readArrayBytes(tree[kPedals], pedals))
        return false;

    for (int i = 0; i < PedalSlotCount; ++i)
    {
        const auto raw = pedals[static_cast<size_t>(i)];
        if (raw > static_cast<uint8_t>(DspModuleType::RESERVED_REMOVED_OCTAVER)
            && raw != static_cast<uint8_t>(DspModuleType::RESERVED_REMOVED_OCTAVER))
            return false;
        if (raw == static_cast<uint8_t>(DspModuleType::RESERVED_REMOVED_OCTAVER))
            state.pedalSlots[static_cast<size_t>(i)] = DspModuleType::BYPASS;
        else
            state.pedalSlots[static_cast<size_t>(i)] = static_cast<DspModuleType>(raw);
    }

    const auto* routing = tree[kRouting].getBinaryData();
    if (routing == nullptr || routing->getSize() > PedalSlotCount)
        return false;
    state.manualRouting.fill(0);
    state.manualRoutingSize = static_cast<uint8_t>(routing->getSize());
    if (state.manualRoutingSize > 0)
        std::memcpy(state.manualRouting.data(), routing->getData(), routing->getSize());

    std::array<bool, PedalSlotCount> routingSeen{};
    for (int i = 0; i < state.manualRoutingSize; ++i)
    {
        const auto slot = state.manualRouting[static_cast<size_t>(i)];
        if (slot >= PedalSlotCount || routingSeen[slot])
            return false;
        routingSeen[slot] = true;
    }

    const auto overrideMask = static_cast<int64_t>(tree[kOverrideMask]);
    const auto linkFlags = static_cast<int64_t>(tree[kLinkFlags]);
    const int barCount = static_cast<int>(tree[kBarCount]);
    const int sectionStart = static_cast<int>(tree[kSectionStart]);
    const int manualMode = static_cast<int>(tree[kManualMode]);
    if (overrideMask < 0 || linkFlags < 0
        || overrideMask > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())
        || linkFlags > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())
        || barCount < 1 || barCount > 8
        || sectionStart < 0 || sectionStart > 7
        || manualMode < 0 || manualMode > 1
        || (overrideMask & ~static_cast<int64_t>((uint32_t{ 1 } << TotalKnobs) - 1u)) != 0
        || (linkFlags & ~static_cast<int64_t>((uint32_t{ 1 } << TotalKnobs) - 1u)) != 0)
        return false;

    state.overrideMask = static_cast<uint32_t>(overrideMask);
    state.barCount = static_cast<uint8_t>(barCount);
    state.sectionStartBar = static_cast<uint8_t>(sectionStart);
    state.manualMode = static_cast<uint8_t>(manualMode);
    state.inputGain = static_cast<float>(tree[kInputGain]);
    state.outputGain = static_cast<float>(tree[kOutputGain]);
    state.linkFlags = static_cast<uint32_t>(static_cast<int64_t>(tree[kLinkFlags]));

    if (!std::isfinite(state.inputGain) || !std::isfinite(state.outputGain)
        || !finiteArray(state.knobValues) || !finiteArray(state.pedalGains)
        || !finiteArray(state.linkRangeMins) || !finiteArray(state.linkRangeMaxs)
        || !finiteArray(state.manualEnvelope))
        return false;
    for (size_t i = 0; i < state.linkRangeMins.size(); ++i)
    {
        float mn = std::clamp(state.linkRangeMins[i], 0.0f, 1.0f);
        float mx = std::clamp(state.linkRangeMaxs[i], 0.0f, 1.0f);
        if (mx < mn + 0.05f) mx = std::min(1.0f, mn + 0.05f);
        if (mn > mx - 0.05f) mn = std::max(0.0f, mx - 0.05f);
        state.linkRangeMins[i] = mn;
        state.linkRangeMaxs[i] = mx;
    }

    return true;
}

bool serializeTree(const juce::ValueTree& tree, juce::MemoryBlock& output)
{
    if (auto xml = tree.createXml())
    {
        juce::AudioProcessor::copyXmlToBinary(*xml, output);
        return true;
    }
    return false;
}

bool deserializeTree(const void* data, size_t size, StateSerializer::DocumentType type, juce::ValueTree& tree)
{
    if (data == nullptr || size == 0
        || size > static_cast<size_t>(std::numeric_limits<int>::max()))
        return false;

    auto xml = juce::AudioProcessor::getXmlFromBinary(data, static_cast<int>(size));
    if (xml == nullptr)
        return false;

    tree = juce::ValueTree::fromXml(*xml);
    const int version = static_cast<int>(tree[kVersion]);
    return tree.isValid()
        && tree.getType() == kRoot
        && version >= 1 && version <= StateSerializer::SchemaVersion
        && static_cast<int>(tree[kType]) == static_cast<int>(type);
}
}

bool StateSerializer::serializePreset(const PresetState& state, juce::MemoryBlock& outBlob)
{
    juce::ValueTree root(kRoot);
    root.setProperty(kType, static_cast<int>(DocumentType::Preset), nullptr);
    root.setProperty(kVersion, SchemaVersion, nullptr);
    root.addChild(makePresetTree(state), -1, nullptr);
    return serializeTree(root, outBlob);
}

bool StateSerializer::serializeProject(const ProjectState& state, juce::MemoryBlock& outBlob)
{
    juce::ValueTree root(kRoot);
    root.setProperty(kType, static_cast<int>(DocumentType::Project), nullptr);
    root.setProperty(kVersion, SchemaVersion, nullptr);
    root.addChild(makePresetTree(state.preset), -1, nullptr);

    juce::ValueTree session(kSession);
    session.setProperty(kSelectedColour, static_cast<int>(state.session.selectedColour), nullptr);
    session.setProperty(kSelectedTool, static_cast<int>(state.session.selectedTool), nullptr);
    session.setProperty(kSelectedPedal, static_cast<int>(state.session.selectedPedal), nullptr);
    session.setProperty(kBrushSizeIndex, static_cast<int>(state.session.brushSizeIndex), nullptr);
    session.setProperty(kLinkRangeEditEnabled, state.session.linkRangeEditEnabled ? 1 : 0, nullptr);
    root.addChild(session, -1, nullptr);
    return serializeTree(root, outBlob);
}

bool StateSerializer::deserializePreset(const void* data, size_t sizeInBytes, PresetState& outState)
{
    juce::ValueTree root;
    if (!deserializeTree(data, sizeInBytes, DocumentType::Preset, root))
        return false;
    return readPresetTree(root.getChildWithName(kPreset), outState);
}

bool StateSerializer::deserializeProject(const void* data, size_t sizeInBytes, ProjectState& outState)
{
    juce::ValueTree root;
    if (!deserializeTree(data, sizeInBytes, DocumentType::Project, root)
        || !readPresetTree(root.getChildWithName(kPreset), outState.preset))
        return false;

    const auto session = root.getChildWithName(kSession);
    if (!session.isValid())
        return false;

    outState.session.selectedColour = static_cast<uint8_t>(juce::jlimit(0, 12, static_cast<int>(session[kSelectedColour])));
    outState.session.selectedTool = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(session[kSelectedTool])));
    outState.session.selectedPedal = static_cast<int8_t>(juce::jlimit(-1, PedalSlotCount - 1, static_cast<int>(session[kSelectedPedal])));
    outState.session.brushSizeIndex = static_cast<uint8_t>(juce::jlimit(0, 3, static_cast<int>(session[kBrushSizeIndex])));
    outState.session.linkRangeEditEnabled = session.hasProperty(kLinkRangeEditEnabled)
        ? static_cast<int>(session[kLinkRangeEditEnabled]) != 0 : false;
    return true;
}
