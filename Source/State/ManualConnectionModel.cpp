#include "ManualConnectionModel.h"

void ManualConnectionModel::clear()
{
    m_edges.clear();
    m_dawIn.fill(false);
    m_dawOut.fill(false);
}

void ManualConnectionModel::addEdge(uint8_t src, uint8_t dst)
{
    m_edges.push_back(std::make_pair(src, dst));
}

void ManualConnectionModel::removeEdge(size_t index)
{
    if (index < m_edges.size())
        m_edges.erase(m_edges.begin() + static_cast<ptrdiff_t>(index));
}

void ManualConnectionModel::removeEdgesWithSource(uint8_t src)
{
    removeIf([src](const auto& e) { return e.first == src; });
}

void ManualConnectionModel::removeEdgesWithDestination(uint8_t dst)
{
    removeIf([dst](const auto& e) { return e.second == dst; });
}

void ManualConnectionModel::removeAllEdgesForSlot(uint8_t slot)
{
    removeIf([slot](const auto& e) { return e.first == slot || e.second == slot; });
}

void ManualConnectionModel::removeConflictingEdges(uint8_t src, uint8_t dst)
{
    removeIf([src, dst](const auto& e) { return e.first == src || e.second == dst; });
}

void ManualConnectionModel::setDawIn(uint8_t pedal)
{
    m_dawIn.fill(false);
    m_dawIn[static_cast<size_t>(pedal)] = true;
}

void ManualConnectionModel::clearDawIn(uint8_t pedal)
{
    if (pedal < PedalSlotCount)
        m_dawIn[static_cast<size_t>(pedal)] = false;
}

bool ManualConnectionModel::hasDawIn(uint8_t pedal) const
{
    return pedal < PedalSlotCount && m_dawIn[static_cast<size_t>(pedal)];
}

int ManualConnectionModel::dawInPedal() const
{
    for (uint8_t p = 0; p < PedalSlotCount; ++p)
        if (m_dawIn[static_cast<size_t>(p)])
            return static_cast<int>(p);
    return -1;
}

void ManualConnectionModel::setDawOut(uint8_t pedal)
{
    m_dawOut.fill(false);
    m_dawOut[static_cast<size_t>(pedal)] = true;
}

void ManualConnectionModel::clearDawOut(uint8_t pedal)
{
    if (pedal < PedalSlotCount)
        m_dawOut[static_cast<size_t>(pedal)] = false;
}

bool ManualConnectionModel::hasDawOut(uint8_t pedal) const
{
    return pedal < PedalSlotCount && m_dawOut[static_cast<size_t>(pedal)];
}

int ManualConnectionModel::dawOutPedal() const
{
    for (uint8_t p = 0; p < PedalSlotCount; ++p)
        if (m_dawOut[static_cast<size_t>(p)])
            return static_cast<int>(p);
    return -1;
}

std::vector<uint8_t> ManualConnectionModel::deriveRoutingOrder() const
{
    std::vector<uint8_t> routing;
    for (const auto& edge : m_edges)
    {
        if (std::find(routing.begin(), routing.end(), edge.first) == routing.end())
            routing.push_back(edge.first);
        if (std::find(routing.begin(), routing.end(), edge.second) == routing.end())
            routing.push_back(edge.second);
    }

    int inPedal = dawInPedal();
    if (inPedal >= 0)
    {
        auto it = std::find(routing.begin(), routing.end(), static_cast<uint8_t>(inPedal));
        if (it == routing.end())
            routing.insert(routing.begin(), static_cast<uint8_t>(inPedal));
        else if (it != routing.begin())
        {
            routing.erase(it);
            routing.insert(routing.begin(), static_cast<uint8_t>(inPedal));
        }
    }

    int outPedal = dawOutPedal();
    if (outPedal >= 0)
    {
        auto it = std::find(routing.begin(), routing.end(), static_cast<uint8_t>(outPedal));
        if (it == routing.end())
            routing.push_back(static_cast<uint8_t>(outPedal));
        else if (it != routing.end() - 1)
        {
            routing.erase(it);
            routing.push_back(static_cast<uint8_t>(outPedal));
        }
    }

    return routing;
}
