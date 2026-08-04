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
        const PedalAssetPayload* expected = nullptr;
        if (!m_singlePtr.compare_exchange_strong(expected, ptr, std::memory_order_release))
        {
            expected = nullptr;
            if (!m_overflowPtr.compare_exchange_strong(expected, ptr, std::memory_order_release))
            {
                // both overflow slots occupied — silently drop. audio thread must never deallocate.
            }
        }
    }
    void drain();

private:
    static constexpr int kCapacity = 16;
    std::array<const PedalAssetPayload*, kCapacity> m_queue{};
    std::atomic<int> m_writeIndex{0};
    std::atomic<int> m_readIndex{0};
    std::atomic<const PedalAssetPayload*> m_singlePtr{nullptr};
    std::atomic<const PedalAssetPayload*> m_overflowPtr{nullptr};
};
