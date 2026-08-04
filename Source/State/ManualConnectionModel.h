#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <utility>
#include "Core/DrawdioConstants.h"

class ManualConnectionModel
{
public:
    void clear();

    void addEdge(uint8_t src, uint8_t dst);
    void removeEdge(size_t index);
    void removeEdgesWithSource(uint8_t src);
    void removeEdgesWithDestination(uint8_t dst);
    void removeAllEdgesForSlot(uint8_t slot);
    void removeConflictingEdges(uint8_t src, uint8_t dst);

    const auto& edges() const { return m_edges; }
    size_t edgeCount() const { return m_edges.size(); }

    void setDawIn(uint8_t pedal);
    void clearDawIn(uint8_t pedal);
    bool hasDawIn(uint8_t pedal) const;
    int dawInPedal() const;

    void setDawOut(uint8_t pedal);
    void clearDawOut(uint8_t pedal);
    bool hasDawOut(uint8_t pedal) const;
    int dawOutPedal() const;

    std::vector<uint8_t> deriveRoutingOrder() const;

private:
    template <typename Pred>
    void removeIf(Pred&& pred)
    {
        m_edges.erase(std::remove_if(m_edges.begin(), m_edges.end(),
            std::forward<Pred>(pred)), m_edges.end());
    }

    std::vector<std::pair<uint8_t, uint8_t>> m_edges;
    std::array<bool, PedalSlotCount> m_dawIn{};
    std::array<bool, PedalSlotCount> m_dawOut{};
};
