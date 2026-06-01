#include "CompilerThread.h"
#include "CompilerEngine.h"
#include <chrono>
#include <memory>
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
    m_slotFull.store(false, std::memory_order_relaxed);
    m_thread = std::thread(&CompilerThread::threadFunc, this, std::ref(queue), std::ref(debouncer));
}

void CompilerThread::stop()
{
    m_running.store(false);
    m_cv.notify_one();
    if (m_thread.joinable())
        m_thread.join();
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

bool CompilerThread::hasCompiledResult() const
{
    return m_slotFull.load(std::memory_order_acquire);
}

std::shared_ptr<PedalAssetPayload> CompilerThread::getCompiledPayloadPtr()
{
    if (!m_slotFull.load(std::memory_order_acquire))
        return nullptr;

    std::lock_guard<std::mutex> lock(m_slotMutex);
    auto ptr = std::move(m_slot);
    m_slotFull.store(false, std::memory_order_release);
    return ptr;
}

void CompilerThread::threadFunc(CanvasMessageQueue& queue, PenDebouncer& debouncer)
{
    while (m_running.load())
    {
        {
            std::unique_lock<std::mutex> lock(m_cvMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(5), [&]() {
                return !m_running.load() || (debouncer.isIdle() && queue.hasMessage());
            });
        }

        if (!m_running.load())
            break;

        if (!debouncer.isIdle())
            continue;

        CanvasMessageQueue::CanvasMessage msg;
        if (!queue.popSnapshot(msg))
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

        // Only publish if the previous result has been consumed.
        if (!m_slotFull.load(std::memory_order_acquire))
        {
            auto newPayload = std::make_shared<PedalAssetPayload>(
                compileCanvas(msg.gridSnapshot.data(), slots, manualRouting, existingParams));
            
            std::lock_guard<std::mutex> lock(m_slotMutex);
            m_slot = std::move(newPayload);
            m_slotFull.store(true, std::memory_order_release);
        }
    }
}
