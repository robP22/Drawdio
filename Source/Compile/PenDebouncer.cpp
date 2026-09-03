#include "Compile/PenDebouncer.h"

void PenDebouncer::penDown()
{
    m_penIsDown.store(true, std::memory_order_release);
}

void PenDebouncer::penUp()
{
    m_penIsDown.store(false, std::memory_order_release);
}

bool PenDebouncer::isIdle() const
{
    return !m_penIsDown.load(std::memory_order_acquire);
}
