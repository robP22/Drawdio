#include "CompilerThread.h"
#include "Compile/CompilerEngine.h"
#include <chrono>


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
    delete m_slot.exchange(nullptr, std::memory_order_acq_rel);
}

void CompilerThread::notify()
{
    m_cv.notify_one();
}

void CompilerThread::setPedalSlots(const std::vector<DspModuleType>& slots)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_pedalSlots = slots;
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
    return m_slot.load(std::memory_order_acquire) != nullptr;
}

PedalAssetPayload* CompilerThread::getCompiledPayloadPtr() noexcept
{
    return m_slot.exchange(nullptr, std::memory_order_acq_rel);
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

        delete m_slot.exchange(new PedalAssetPayload(
                                   compileCanvas(*gridSnapshot, slots, manualRouting, existingParams)),
                               std::memory_order_acq_rel);
    }
}
