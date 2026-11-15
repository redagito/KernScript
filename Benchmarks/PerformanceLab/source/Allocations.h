#pragma once

#include <cstddef>

namespace allocations
{
struct Snapshot
{
    std::size_t liveBytes;
    std::size_t liveAllocations;
    std::size_t totalRequestedBytes;
    std::size_t totalAllocations;
    std::size_t highWaterBytes;
};

Snapshot snapshot() noexcept;
void resetPeak() noexcept;
} // namespace allocations
