#include <catch2/catch_test_macros.hpp>

#include <array>

#include "Compile/CanvasMessageQueue.h"

TEST_CASE("Canvas queue preserves occupied messages when full", "[compiler]")
{
    CanvasMessageQueue queue;
    std::array<uint8_t, TotalCells> grid{};

    for (uint32_t revision = 1; revision < CanvasMessageQueue::QueueCapacity; ++revision)
    {
        grid.fill(static_cast<uint8_t>(revision));
        queue.pushSnapshot(grid.data(), DirtyRowMask{}, revision);
    }

    grid.fill(99);
    queue.pushSnapshot(grid.data(), DirtyRowMask{}, 99);

    for (uint32_t revision = 1; revision < CanvasMessageQueue::QueueCapacity; ++revision)
    {
        const auto* message = queue.popMessage();
        REQUIRE(message != nullptr);
        REQUIRE(message->revision == revision);
        REQUIRE(message->gridSnapshot.front() == revision);
    }

    REQUIRE(queue.popMessage() == nullptr);
    REQUIRE(queue.latestRevision() == CanvasMessageQueue::QueueCapacity - 1);
}
