#include "CanvasMessageQueue.h"
#include <cstring>

CanvasMessageQueue::CanvasMessageQueue()
    : m_writeIndex(0),
      m_readIndex(0),
      m_hasMessage(false)
{
}

void CanvasMessageQueue::pushSnapshot(const uint8_t* gridData)
{
    int writeIdx = m_writeIndex.load(std::memory_order_relaxed);
    int nextWrite = (writeIdx + 1) % QueueCapacity;

    if (nextWrite == m_readIndex.load(std::memory_order_acquire))
        return;

    std::memcpy(m_queue[writeIdx].gridSnapshot.data(), gridData, PayloadSize);

    m_writeIndex.store(nextWrite, std::memory_order_release);
    m_hasMessage.store(true, std::memory_order_release);
}

bool CanvasMessageQueue::popSnapshot(CanvasMessage& outMessage)
{
    if (!m_hasMessage.load(std::memory_order_acquire))
        return false;

    int readIdx = m_readIndex.load(std::memory_order_relaxed);

    if (readIdx == m_writeIndex.load(std::memory_order_acquire))
        return false;

    outMessage = m_queue[readIdx];
    m_readIndex.store((readIdx + 1) % QueueCapacity, std::memory_order_release);

    if (m_readIndex.load(std::memory_order_relaxed) == m_writeIndex.load(std::memory_order_relaxed))
        m_hasMessage.store(false, std::memory_order_release);

    return true;
}

bool CanvasMessageQueue::hasMessage() const
{
    return m_hasMessage.load(std::memory_order_acquire);
}
