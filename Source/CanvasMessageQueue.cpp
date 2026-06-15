#include "CanvasMessageQueue.h"
#include <cstring>

CanvasMessageQueue::CanvasMessageQueue() noexcept
    : m_writeIndex(0),
      m_readIndex(0)
{
}

void CanvasMessageQueue::pushSnapshot(const uint8_t* gridData) noexcept
{
    int writeIdx = m_writeIndex.load(std::memory_order_relaxed);
    int nextWrite = (writeIdx + 1) % QueueCapacity;

    if (nextWrite == m_readIndex.load(std::memory_order_acquire))
        return;

    std::memcpy(m_queue[writeIdx].gridSnapshot.data(), gridData, PayloadSize);

    m_writeIndex.store(nextWrite, std::memory_order_release);
}

const std::array<uint8_t, CanvasMessageQueue::PayloadSize>* CanvasMessageQueue::popSnapshot() noexcept
{
    int readIdx = m_readIndex.load(std::memory_order_relaxed);

    if (readIdx == m_writeIndex.load(std::memory_order_acquire))
        return nullptr;

    auto& result = m_queue[readIdx].gridSnapshot;
    m_readIndex.store((readIdx + 1) % QueueCapacity, std::memory_order_release);

    return &result;
}

bool CanvasMessageQueue::hasMessage() const noexcept
{
    return m_writeIndex.load(std::memory_order_acquire) != m_readIndex.load(std::memory_order_acquire);
}
