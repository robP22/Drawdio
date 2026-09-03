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
};
