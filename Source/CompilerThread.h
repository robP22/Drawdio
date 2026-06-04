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
    // Uses atomic flag for lock-free single-producer-single-consumer protocol.
    std::shared_ptr<PedalAssetPayload> m_slot{nullptr};
    std::atomic<bool> m_slotFull{false};

    mutable std::mutex m_configMutex;

    // Atomic slot count for lock-free validation
    std::atomic<size_t> m_slotCount{0};

    std::mutex m_cvMutex;
    std::condition_variable m_cv;
};
