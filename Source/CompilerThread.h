#pragma once
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "PedalStructures.h"
#include "CanvasMessageQueue.h"
#include "PenDebouncer.h"

class CompilerThread
{
public:
    CompilerThread();
    ~CompilerThread();

    void start(CanvasMessageQueue& queue, PenDebouncer& debouncer);
    void stop();

    void setPedalSlots(const std::vector<DspModuleType>& slots);
    void setManualRouting(const std::vector<uint8_t>& routing);
    void setExistingParameters(const std::vector<ParameterDescriptor>& params);

    void notify();

    bool hasCompiledResult() const;
    std::shared_ptr<PedalAssetPayload> getCompiledPayloadPtr();

private:
    void threadFunc(CanvasMessageQueue& queue, PenDebouncer& debouncer);

    std::thread m_thread;
    std::atomic<bool> m_running;

    std::vector<DspModuleType> m_pedalSlots;
    std::vector<uint8_t> m_manualRouting;
    std::vector<ParameterDescriptor> m_existingParams;

    // Thread-safe slot: compiler thread writes, UI/Audio thread reads.
    std::shared_ptr<PedalAssetPayload> m_slot;
    std::atomic<bool> m_slotFull{false};
    mutable std::mutex m_slotMutex;

    mutable std::mutex m_configMutex;

    std::mutex m_cvMutex;
    std::condition_variable m_cv;
};
