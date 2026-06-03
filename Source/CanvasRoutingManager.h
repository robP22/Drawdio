#pragma once

#include <cstdint>
#include <vector>

#include "PedalStructures.h"

class CanvasRoutingManager final
{
public:
    enum class ColorLayer : uint8_t
    {
        Black = 0,
        Blue = 1,
        Green = 2,
        Red = 3,
        White = 4
    };

    struct ColorParameterMapping
    {
        ColorLayer layer;
        uint16_t parameterToken;
    };

    struct ColorPedalRelationship
    {
        ColorLayer layer;
        uint8_t pedalSlot;
    };

    struct RoutingConnection
    {
        uint8_t sourceSlot;
        uint8_t destinationSlot;
    };

    CanvasRoutingManager();

    void setRoutingOrder(const std::vector<uint8_t>& routingOrder);
    void setManualRoutingOrder(const std::vector<uint8_t>& routingOrder);
    void clearManualRouting();

    const std::vector<uint8_t>& getRoutingOrder() const noexcept { return m_effectiveRoutingOrder; }
    std::vector<RoutingConnection> getConnections() const;
    std::vector<uint8_t> makeRoutingWithConnection(int sourceSlot, int destinationSlot) const;

    const std::vector<ColorParameterMapping>& getColorParameterMappings() const noexcept { return m_colorParameterMappings; }
    const std::vector<ColorPedalRelationship>& getColorPedalRelationships() const noexcept { return m_colorPedalRelationships; }
    void setColorPedalRelationships(std::vector<ColorPedalRelationship> relationships);

private:
    static std::vector<uint8_t> sanitizeRouting(const std::vector<uint8_t>& routingOrder);
    void rebuildColorPedalRelationships();

    std::vector<uint8_t> m_effectiveRoutingOrder;
    std::vector<uint8_t> m_manualRoutingOrder;
    std::vector<ColorParameterMapping> m_colorParameterMappings;
    std::vector<ColorPedalRelationship> m_colorPedalRelationships;
};
