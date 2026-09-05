#include "Compile/CanvasMessageQueue.h"
#include <cstring>

CanvasMessageQueue::CanvasMessageQueue()
    : m_queue(std::make_unique<std::array<CanvasMessage, QueueCapacity>>()),
      m_cachedMessage(std::make_unique<CanvasMessage>()),
      m_writeIndex(0),
      m_readIndex(0)
{
}

bool CanvasMessageQueue::pushSnapshot(const uint8_t* gridData) noexcept
{
    DirtyRowMask allRows;
    allRows.fill(~uint64_t{ 0 });
    return pushSnapshot(gridData, allRows, 0);
}

bool CanvasMessageQueue::pushSnapshot(const uint8_t* gridData,
                                      const DirtyRowMask& dirtyRows,
                                      uint32_t revision) noexcept
{
    static const std::vector<DspModuleType> noSlots;
    static const std::vector<uint8_t> noRouting;
    static const std::vector<ParameterDescriptor> noParams;
    return pushSnapshot(gridData, dirtyRows, revision, noSlots, noRouting, noParams);
}

bool CanvasMessageQueue::pushSnapshot(const uint8_t* gridData,
                                      const DirtyRowMask& dirtyRows,
                                      uint32_t revision,
                                      const std::vector<DspModuleType>& pedalSlots,
                                      const std::vector<uint8_t>& manualRouting,
                                      const std::vector<ParameterDescriptor>& existingParams) noexcept
{
    int writeIdx = m_writeIndex.load(std::memory_order_relaxed);
    int nextWrite = (writeIdx + 1) % QueueCapacity;

    if (nextWrite == m_readIndex.load(std::memory_order_acquire))
        return false;

    m_latestRevision.store(revision, std::memory_order_release);
    std::memcpy((*m_queue)[writeIdx].gridSnapshot.data(), gridData, PayloadSize);
    (*m_queue)[writeIdx].dirtyRows = dirtyRows;
    (*m_queue)[writeIdx].revision = revision;
    auto& message = (*m_queue)[writeIdx];
    message.pedalSlots.fill(DspModuleType::BYPASS);
    message.manualRouting.fill(0);
    message.existingParams = {};
    message.manualRoutingSize = static_cast<uint8_t>(std::min(manualRouting.size(), message.manualRouting.size()));
    message.existingParamsSize = static_cast<uint8_t>(std::min(existingParams.size(), message.existingParams.size()));
    for (size_t i = 0; i < std::min(pedalSlots.size(), message.pedalSlots.size()); ++i)
        message.pedalSlots[i] = pedalSlots[i];
    for (size_t i = 0; i < message.manualRoutingSize; ++i)
        message.manualRouting[i] = manualRouting[i];
    for (size_t i = 0; i < message.existingParamsSize; ++i)
        message.existingParams[i] = existingParams[i];

    m_writeIndex.store(nextWrite, std::memory_order_release);
    return true;
}

const CanvasMessageQueue::CanvasMessage* CanvasMessageQueue::popMessage() noexcept
{
    int readIdx = m_readIndex.load(std::memory_order_relaxed);

    if (readIdx == m_writeIndex.load(std::memory_order_acquire))
        return nullptr;

    *m_cachedMessage = (*m_queue)[readIdx];
    m_readIndex.store((readIdx + 1) % QueueCapacity, std::memory_order_release);

    return m_cachedMessage.get();
}

const std::array<uint8_t, CanvasMessageQueue::PayloadSize>* CanvasMessageQueue::popSnapshot() noexcept
{
    const auto* message = popMessage();
    return message != nullptr ? &message->gridSnapshot : nullptr;
}

bool CanvasMessageQueue::hasMessage() const noexcept
{
    return m_writeIndex.load(std::memory_order_acquire) != m_readIndex.load(std::memory_order_acquire);
}
