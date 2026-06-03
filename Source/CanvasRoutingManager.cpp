#include "CanvasRoutingManager.h"

#include <algorithm>
#include <array>
#include <utility>

CanvasRoutingManager::CanvasRoutingManager()
    : m_colorParameterMappings {
          { ColorLayer::Blue, ParamToken::Wet },
          { ColorLayer::Green, ParamToken::Dry },
          { ColorLayer::Red, ParamToken::Volume },
          { ColorLayer::White, ParamToken::Effect }
      }
{
}

void CanvasRoutingManager::setRoutingOrder(const std::vector<uint8_t>& routingOrder)
{
    m_effectiveRoutingOrder = sanitizeRouting(routingOrder);
    rebuildColorPedalRelationships();
}

void CanvasRoutingManager::setManualRoutingOrder(const std::vector<uint8_t>& routingOrder)
{
    m_manualRoutingOrder = sanitizeRouting(routingOrder);
    setRoutingOrder(m_manualRoutingOrder);
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

std::vector<uint8_t> CanvasRoutingManager::makeRoutingWithConnection(int sourceSlot, int destinationSlot) const
{
    if (sourceSlot < 0 || sourceSlot >= PedalSlotCount
        || destinationSlot < 0 || destinationSlot >= PedalSlotCount
        || sourceSlot == destinationSlot)
    {
        return m_effectiveRoutingOrder;
    }

    auto routing = m_effectiveRoutingOrder;
    const auto source = static_cast<uint8_t>(sourceSlot);
    const auto destination = static_cast<uint8_t>(destinationSlot);

    if (std::find(routing.begin(), routing.end(), source) == routing.end())
        routing.push_back(source);

    routing.erase(std::remove(routing.begin(), routing.end(), destination), routing.end());

    auto sourceIt = std::find(routing.begin(), routing.end(), source);
    if (sourceIt != routing.end())
        routing.insert(sourceIt + 1, destination);

    return sanitizeRouting(routing);
}

void CanvasRoutingManager::setColorPedalRelationships(std::vector<ColorPedalRelationship> relationships)
{
    m_colorPedalRelationships = std::move(relationships);
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

void CanvasRoutingManager::rebuildColorPedalRelationships()
{
    static constexpr std::array<ColorLayer, 4> layers {
        ColorLayer::Blue,
        ColorLayer::Green,
        ColorLayer::Red,
        ColorLayer::White
    };

    m_colorPedalRelationships.clear();
    m_colorPedalRelationships.reserve(m_effectiveRoutingOrder.size());

    for (size_t i = 0; i < m_effectiveRoutingOrder.size(); ++i)
    {
        m_colorPedalRelationships.push_back({
            layers[i % layers.size()],
            m_effectiveRoutingOrder[i]
        });
    }
}
