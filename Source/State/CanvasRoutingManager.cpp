#include "CanvasRoutingManager.h"

#include <algorithm>

CanvasRoutingManager::CanvasRoutingManager()
{
}

void CanvasRoutingManager::setRoutingOrder(const std::vector<uint8_t>& routingOrder)
{
    m_effectiveRoutingOrder = sanitizeRouting(routingOrder);
}

void CanvasRoutingManager::clearManualRouting()
{
    m_manualRoutingOrder.clear();
    setRoutingOrder({});
}

std::vector<CanvasRoutingManager::RoutingConnection> CanvasRoutingManager::getConnections() const
{
    std::vector<RoutingConnection> connections;
    if (m_effectiveRoutingOrder.size() < 2)
        return connections;

    connections.reserve(m_effectiveRoutingOrder.size() - 1);
    for (size_t i = 0; i + 1 < m_effectiveRoutingOrder.size(); ++i)
    {
        connections.push_back({ m_effectiveRoutingOrder[i], m_effectiveRoutingOrder[i + 1] });
    }

    return connections;
}

std::vector<uint8_t> CanvasRoutingManager::sanitizeRouting(const std::vector<uint8_t>& routingOrder)
{
    std::vector<uint8_t> sanitized;
    sanitized.reserve(routingOrder.size());

    for (auto slot : routingOrder)
    {
        if (slot >= PedalSlotCount)
            continue;

        if (std::find(sanitized.begin(), sanitized.end(), slot) == sanitized.end())
            sanitized.push_back(slot);
    }

    return sanitized;
}
