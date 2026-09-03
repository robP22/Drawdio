#pragma once

#include <cstdint>
#include <vector>

#include "Core/DrawdioConstants.h"

class CanvasRoutingManager final
{
public:
    struct RoutingConnection
    {
        uint8_t sourceSlot;
        uint8_t destinationSlot;
    };

    CanvasRoutingManager();

    void setRoutingOrder(const std::vector<uint8_t>& routingOrder);

    const std::vector<uint8_t>& getRoutingOrder() const noexcept { return m_effectiveRoutingOrder; }
    std::vector<RoutingConnection> getConnections() const;

private:
    static std::vector<uint8_t> sanitizeRouting(const std::vector<uint8_t>& routingOrder);

    std::vector<uint8_t> m_effectiveRoutingOrder;
};
