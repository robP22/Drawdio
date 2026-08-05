#include "ReleaseQueue.h"

ReleaseQueue::ReleaseQueue() = default;

void ReleaseQueue::push(const PedalAssetPayload* ptr)
{
    int wIdx = m_writeIndex.load(std::memory_order_relaxed);
    int nextW = (wIdx + 1) % kCapacity;
    if (nextW == m_readIndex.load(std::memory_order_acquire))
    {
        delete ptr;
        return;
    }
    m_queue[static_cast<size_t>(wIdx)] = ptr;
    m_writeIndex.store(nextW, std::memory_order_release);
}

void ReleaseQueue::drain()
{
    for (auto& slot : m_singleSlots)
    {
        auto* ptr = slot.exchange(nullptr, std::memory_order_acq_rel);
        delete ptr;
    }

    int rIdx = m_readIndex.load(std::memory_order_relaxed);
    int wIdx = m_writeIndex.load(std::memory_order_acquire);
    while (rIdx != wIdx)
    {
        delete m_queue[static_cast<size_t>(rIdx)];
        rIdx = (rIdx + 1) % kCapacity;
    }
    m_readIndex.store(rIdx, std::memory_order_release);
}
