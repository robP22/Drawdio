#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include "Core/CompiledPedalConfig.h"

class ReleaseQueue
{
public:
    ReleaseQueue();

    void push(const PedalAssetPayload* ptr);
    void pushSingle(const PedalAssetPayload* ptr) {
        for (int i = 0; i < kSingleSlots; ++i)
        {
            const PedalAssetPayload* expected = nullptr;
            if (m_singleSlots[static_cast<size_t>(i)].compare_exchange_strong(expected, ptr,
                    std::memory_order_release, std::memory_order_relaxed))
                return;
        }
        const auto* prev = m_overflow.exchange(ptr, std::memory_order_acq_rel);
        if (prev)
            m_droppedCount.fetch_add(1, std::memory_order_relaxed);
    }
    void drain();
    uint32_t droppedCount() const { return m_droppedCount.load(std::memory_order_relaxed); }

private:
    static constexpr int kCapacity = 16;
    static constexpr int kSingleSlots = 8;
    std::array<const PedalAssetPayload*, kCapacity> m_queue{};
    std::atomic<int> m_writeIndex{0};
    std::atomic<int> m_readIndex{0};
    std::array<std::atomic<const PedalAssetPayload*>, kSingleSlots> m_singleSlots{};
    std::atomic<const PedalAssetPayload*> m_overflow{nullptr};
    std::atomic<uint32_t> m_droppedCount{0};
};
