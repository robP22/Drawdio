#pragma once
#include <atomic>
#include <cstdint>

class PenDebouncer
{
public:
    void penDown();
    void penUp();
    bool isIdle() const;

private:
    std::atomic<bool> m_penIsDown{false};
    std::atomic<bool> m_debouncePending{false};
    std::atomic<int64_t> m_penUpTimeMs{0};
    static constexpr int DebounceMs = 300;
};
