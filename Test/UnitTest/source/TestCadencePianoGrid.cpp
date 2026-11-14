#include "../../../Samples/Cadence/source/PianoGrid.h"
#include <catch2/catch_test_macros.hpp>
#include <utility>
#include <vector>

TEST_CASE("Cadence clicks select the same cell that is drawn in an uneven piano grid", "[unit][samples][cadence]")
{
    for (const auto [count, extent] :
         std::vector<std::pair<int, int>>{{72, 500}, {72, 491}, {128, 1140}, {64, 901}, {32, 800}})
    {
        for (int cell = 0; cell < count; ++cell)
        {
            CAPTURE(count, extent, cell);
            const int first = cadence::gridBoundary(cell, count, extent);
            const int last = cadence::gridBoundary(cell + 1, count, extent);
            REQUIRE(cadence::gridCell(first + (last - first) / 2, count, extent) == cell);
            REQUIRE(cadence::gridCell(first, count, extent) == cell);
            REQUIRE(cadence::gridCell(last - 1, count, extent) == cell);
        }
        REQUIRE(cadence::gridCell(-1, count, extent) == -1);
        REQUIRE(cadence::gridCell(extent, count, extent) == -1);
    }
    const int top = cadence::gridBoundary(47, 72, 500);
    const int bottom = cadence::gridBoundary(48, 72, 500);
    REQUIRE(107 - cadence::gridCell((top + bottom) / 2, 72, 500) == 60);
}
