#include <catch2/catch_test_macros.hpp>

#include "UI/EditorLayout.h"

TEST_CASE("Editor layout preserves the supported design aspect ratio", "[layout]")
{
    const auto minimum = EditorLayout::calculate({ 0, 0, 1050, 675 });
    const auto design = EditorLayout::calculate({ 0, 0, 1400, 900 });
    const auto maximum = EditorLayout::calculate({ 0, 0, 1750, 1125 });

    REQUIRE(minimum.content.getWidth() == 1050);
    REQUIRE(minimum.content.getHeight() == 675);
    REQUIRE(design.content.getWidth() == 1400);
    REQUIRE(design.content.getHeight() == 900);
    REQUIRE(maximum.content.getWidth() == 1750);
    REQUIRE(maximum.content.getHeight() == 1125);
    REQUIRE(minimum.pixelCanvas.getWidth() == minimum.pixelCanvas.getHeight());
    REQUIRE(design.pixelCanvas.getWidth() == design.pixelCanvas.getHeight());
    REQUIRE(maximum.pixelCanvas.getWidth() == maximum.pixelCanvas.getHeight());
}

TEST_CASE("Editor layout centers content when host bounds are not proportional", "[layout]")
{
    const auto layout = EditorLayout::calculate({ 0, 0, 1600, 900 });
    REQUIRE(layout.content.getWidth() == 1400);
    REQUIRE(layout.content.getHeight() == 900);
    REQUIRE(layout.content.getX() == 100);
    REQUIRE(layout.content.getY() == 0);
}

TEST_CASE("Editor layout keeps adjacent regions on shared pixel boundaries", "[layout]")
{
    for (const auto bounds : { juce::Rectangle<int>(1051, 677),
                               juce::Rectangle<int>(1237, 811),
                               juce::Rectangle<int>(1749, 1124) })
    {
        const auto layout = EditorLayout::calculate(bounds);
        REQUIRE(layout.canvasArea.getRight() == layout.pedalboardArea.getX());
        REQUIRE(layout.pedalboardArea.getBottom() == layout.bottomBar.getY());
        REQUIRE(layout.pixelCanvas.getWidth() == layout.pixelCanvas.getHeight());
        REQUIRE(layout.content.getRight() <= bounds.getRight());
        REQUIRE(layout.content.getBottom() <= bounds.getBottom());
    }
}
