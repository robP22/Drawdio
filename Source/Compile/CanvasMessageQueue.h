#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "Core/DrawdioConstants.h"
#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"

class CanvasMessageQueue
{
public:
    static constexpr int PayloadSize = TotalCells;

    struct CanvasMessage
    {
        std::array<uint8_t, PayloadSize> gridSnapshot;
        DirtyRowMask dirtyRows{};
        uint32_t revision = 0;
        std::array<DspModuleType, PedalSlotCount> pedalSlots{};
        std::array<uint8_t, PedalSlotCount> manualRouting{};
        std::array<ParameterDescriptor, TotalKnobs> existingParams{};
        uint8_t manualRoutingSize = 0;
        uint8_t existingParamsSize = 0;
    };

    static constexpr int QueueCapacity = 8;

    CanvasMessageQueue();
    ~CanvasMessageQueue() noexcept = default;

    bool pushSnapshot(const uint8_t* gridData) noexcept;
    bool pushSnapshot(const uint8_t* gridData, const DirtyRowMask& dirtyRows,
                      uint32_t revision) noexcept;
    bool pushSnapshot(const uint8_t* gridData, const DirtyRowMask& dirtyRows,
                      uint32_t revision,
                      const std::vector<DspModuleType>& pedalSlots,
                      const std::vector<uint8_t>& manualRouting,
                      const std::vector<ParameterDescriptor>& existingParams) noexcept;
    const CanvasMessage* popMessage() noexcept;
    const std::array<uint8_t, PayloadSize>* popSnapshot() noexcept;
    bool hasMessage() const noexcept;
    uint32_t latestRevision() const noexcept { return m_latestRevision.load(std::memory_order_acquire); }

private:
    std::unique_ptr<std::array<CanvasMessage, QueueCapacity>> m_queue;
    std::unique_ptr<CanvasMessage> m_cachedMessage;
    std::atomic<uint32_t> m_latestRevision{0};
    std::atomic<int> m_writeIndex;
    std::atomic<int> m_readIndex;
};
