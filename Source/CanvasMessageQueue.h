#pragma once
#include <array>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <chrono>
#include "PedalStructures.h"

class CanvasMessageQueue
{
public:
    static constexpr int PayloadSize = TotalCells;

    struct CanvasMessage
    {
        std::array<uint8_t, PayloadSize> gridSnapshot;
        int64_t timestamp;
    };

    static constexpr int QueueCapacity = 8;

    CanvasMessageQueue();
    ~CanvasMessageQueue() = default;

    void pushSnapshot(const uint8_t* gridData);
    bool popSnapshot(CanvasMessage& outMessage);
    bool hasMessage() const;

private:
    std::array<CanvasMessage, QueueCapacity> m_queue;
    std::atomic<int> m_writeIndex;
    std::atomic<int> m_readIndex;
    std::atomic<bool> m_hasMessage;
};
