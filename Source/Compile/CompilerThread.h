#pragma once
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>
#include "Compile/CanvasMessageQueue.h"
#include "Core/CompiledPedalConfig.h"
#include "Compile/CanvasGraphAnalyzer.h"
#include "Core/DspModuleType.h"
#include "Core/ParameterTypes.h"
#include "Compile/PenDebouncer.h"

class CompilerThread
{
public:
    CompilerThread();
    ~CompilerThread();

    void start(CanvasMessageQueue& queue, PenDebouncer& debouncer);
    void stop();

    void notify();
    void setResultAvailableCallback(std::function<void()> callback);

    bool hasCompiledResult() const noexcept;
    PedalAssetPayload* getCompiledPayloadPtr() noexcept;

private:
    void threadFunc(CanvasMessageQueue& queue, PenDebouncer& debouncer);

    std::thread m_thread;
    std::atomic<bool> m_running;

    std::atomic<PedalAssetPayload*> m_slot{nullptr};
    std::unique_ptr<CanvasGraphAnalyzer> m_graphAnalyzer;
    std::function<void()> m_resultAvailableCallback;

    mutable std::mutex m_configMutex;

    std::mutex m_cvMutex;
    std::condition_variable m_cv;

    // Serializes start()/stop(): the host may race releaseResources() against
    // the instance deletion, and two concurrent joins on one std::thread are
    // undefined behavior (a hard block on FL Studio's teardown).
    std::mutex m_stopMutex;
};
