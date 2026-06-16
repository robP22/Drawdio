#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include "PedalStructures.h"

class CanvasMessageQueue
{
public:
    static constexpr int PayloadSize = TotalCells;

    struct CanvasMessage
    {
        std::array<uint8_t, PayloadSize> gridSnapshot;
    };

    static constexpr int QueueCapacity = 8;

    CanvasMessageQueue() noexcept;
    ~CanvasMessageQueue() noexcept = default;

    void pushSnapshot(const uint8_t* gridData) noexcept;
    const std::array<uint8_t, PayloadSize>* popSnapshot() noexcept;
    bool hasMessage() const noexcept;

private:
    std::array<CanvasMessage, QueueCapacity> m_queue;
    std::array<uint8_t, PayloadSize> m_cachedSnapshot;
    std::atomic<int> m_writeIndex;
    std::atomic<int> m_readIndex;
};
