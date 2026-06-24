#include "CompilerThread.h"
#include "CompilerEngine.h"
#include <chrono>
#include <mutex>
#include <utility>

CompilerThread::CompilerThread()
    : m_running(false)
{
}

CompilerThread::~CompilerThread()
{
    stop();
}

void CompilerThread::start(CanvasMessageQueue& queue, PenDebouncer& debouncer)
{
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&CompilerThread::threadFunc, this, std::ref(queue), std::ref(debouncer));
}

void CompilerThread::stop()
{
    m_running.store(false);
    m_cv.notify_one();
    if (m_thread.joinable())
        m_thread.join();
    if (m_slotFull.load(std::memory_order_acquire))
    {
        delete m_slot;
        m_slot = nullptr;
        m_slotFull.store(false, std::memory_order_release);
    }
}

void CompilerThread::notify()
{
    m_cv.notify_one();
}

void CompilerThread::setPedalSlots(const std::vector<DspModuleType>& slots)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_pedalSlots = slots;
    m_slotCount.store(slots.size(), std::memory_order_relaxed);
}

void CompilerThread::setManualRouting(const std::vector<uint8_t>& routing)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_manualRouting = routing;
}

void CompilerThread::setExistingParameters(const std::vector<ParameterDescriptor>& params)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_existingParams = params;
}

bool CompilerThread::hasCompiledResult() const noexcept
{
    return m_slotFull.load(std::memory_order_acquire);
}

const PedalAssetPayload* CompilerThread::getCompiledPayloadPtr() noexcept
{
    bool expected = true;
    if (!m_slotFull.compare_exchange_strong(expected, false, std::memory_order_acq_rel, std::memory_order_acquire))
        return nullptr;

    auto* ptr = m_slot;
    m_slot = nullptr;
    return ptr;
}

void CompilerThread::threadFunc(CanvasMessageQueue& queue, PenDebouncer& debouncer)
{
    while (m_running.load())
    {
        {
            std::unique_lock<std::mutex> lock(m_cvMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(50), [&]() {
                return !m_running.load() || (debouncer.isIdle() && queue.hasMessage());
            });
        }

        if (!m_running.load())
            break;

        if (!debouncer.isIdle())
            continue;

        const auto* gridSnapshot = queue.popSnapshot();
        if (!gridSnapshot)
            continue;

        std::vector<DspModuleType> slots;
        std::vector<uint8_t> manualRouting;
        std::vector<ParameterDescriptor> existingParams;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            slots = m_pedalSlots;
            manualRouting = m_manualRouting;
            existingParams = m_existingParams;
        }

        if (auto* stale = getCompiledPayloadPtr())
            delete stale;
        m_slot = new PedalAssetPayload(
            compileCanvas(*gridSnapshot, slots, manualRouting, existingParams));
        m_slotFull.store(true, std::memory_order_release);
    }
}
