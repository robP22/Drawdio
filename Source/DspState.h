#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include "PedalStructures.h"

constexpr int MaxParameters = 64;
constexpr int MaxRoutingChain = PedalSlotCount + 2;

class DspState
{
public:
    DspState();
    ~DspState() = default;

    DspState(const DspState&) = delete;
    DspState& operator=(const DspState&) = delete;

    // Thread-safe parameter access (UI thread writes, DSP thread reads)
    void setParameterValue(int slot, int paramIdx, float value);
    float getParameterValue(int slot, int paramIdx) const;

    // Parameter change notification
    bool tryMarkDirty();
    bool isDirty() const;
    void clearDirty();

    // UI repaint notification (separate from DSP dirty flag)
    bool consumeNeedsRepaint();
    void requestRepaint();

    // Configuration revision (atomic)
    uint32_t getRevision() const;
    void incrementRevision();

    // Pedal slot types
    void setPedalType(int slot, DspModuleType type);
    DspModuleType getPedalType(int slot) const;

    // Routing
    void setManualRouting(const std::vector<uint8_t>& routing);
    std::vector<uint8_t> getManualRouting() const;

    // Snapshot for DSP (immutable copy read by audio thread)
    struct ParameterSnapshot
    {
        std::array<float, MaxParameters> values;
        std::array<DspModuleType, PedalSlotCount> pedalTypes;
        std::vector<uint8_t> routing;
        uint32_t revision;
    };

    ParameterSnapshot getSnapshot() const;

    // Grid data (for compiler)
    void setGridData(const std::array<uint8_t, TotalCells>& data);
    std::array<uint8_t, TotalCells> getGridData() const;

private:
    std::array<std::atomic<float>, MaxParameters> m_parameters;
    std::atomic<bool> m_dirty{false};
    std::atomic<uint32_t> m_revision{0};
    std::atomic<bool> m_needsRepaint{false};

    std::array<DspModuleType, PedalSlotCount> m_pedalSlots;
    mutable std::mutex m_routingMutex;
    std::vector<uint8_t> m_manualRouting;
    std::array<uint8_t, TotalCells> m_gridData;
};