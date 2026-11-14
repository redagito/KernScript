#pragma once

#include <stdexcept>

namespace cadence
{
// Pixel intervals are [boundary(i), boundary(i+1)). Drawing and hit testing
// share these boundaries even when the viewport is not divisible by its grid.
inline int gridBoundary(int cell, int count, int extent)
{
    if (count < 1 || extent < count || cell < 0 || cell > count)
        throw std::invalid_argument("Invalid piano grid dimensions");
    return cell * extent / count;
}
inline int gridCell(int pixel, int count, int extent)
{
    if (count < 1 || extent < count || pixel < 0 || pixel >= extent)
        return -1;
    return ((pixel + 1) * count - 1) / extent;
}
} // namespace cadence
