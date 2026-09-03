#include "CompilerThread.h"
#include "Compile/CompilerEngine.h"
#include "Effects/DspEffect.h"
#include <chrono>
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
    std::lock_guard<std::mutex> lock(m_stopMutex);
    if (m_running.load()) return;
    if (!m_graphAnalyzer)
        m_graphAnalyzer = std::make_unique<CanvasGraphAnalyzer>();
    m_running.store(true);
    m_thread = std::thread(&CompilerThread::threadFunc, this, std::ref(queue), std::ref(debouncer));
}

void CompilerThread::stop()
{
    std::lock_guard<std::mutex> lock(m_stopMutex);
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

void CompilerThread::setResultAvailableCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_resultAvailableCallback = std::move(callback);
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

        const auto* message = queue.popMessage();
        if (!message)
            continue;

        const uint32_t sourceRevision = message->revision;
        if (sourceRevision != 0 && sourceRevision != queue.latestRevision())
            continue;

        std::vector<DspModuleType> slots(message->pedalSlots.begin(),
                                         message->pedalSlots.begin() + message->pedalSlots.size());
        std::vector<uint8_t> manualRouting(message->manualRouting.begin(),
                                           message->manualRouting.begin() + message->manualRoutingSize);
        std::vector<ParameterDescriptor> existingParams(message->existingParams.begin(),
                                                         message->existingParams.begin() + message->existingParamsSize);

        auto result = compileCanvas(*m_graphAnalyzer, message->gridSnapshot,
                                    message->dirtyRows, message->revision,
                                    slots, manualRouting, existingParams);
        result.sourceRevision = sourceRevision;
        if (sourceRevision != 0 && sourceRevision != queue.latestRevision())
            continue;

        delete m_slot.exchange(new PedalAssetPayload(std::move(result)),
                               std::memory_order_acq_rel);
        std::function<void()> callback;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            callback = m_resultAvailableCallback;
        }
        if (callback)
            callback();
    }
}
