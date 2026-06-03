#include "PenDebouncer.h"

#include <chrono>

void PenDebouncer::penDown()
{
    m_penIsDown.store(true, std::memory_order_release);
    m_debouncePending.store(false, std::memory_order_release);
}

void PenDebouncer::penUp()
{
    m_penIsDown.store(false, std::memory_order_release);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    m_penUpTimeMs.store(now, std::memory_order_release);
    m_debouncePending.store(true, std::memory_order_release);
}

bool PenDebouncer::isIdle() const
{
    if (m_penIsDown.load(std::memory_order_acquire))
        return false;

    if (!m_debouncePending.load(std::memory_order_acquire))
        return true;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    auto then = m_penUpTimeMs.load(std::memory_order_acquire);
    return (now - then) >= DebounceMs;
}
