#include "Allocations.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace
{
// This executable runs single-threaded. Track requested C++ payload bytes;
// the header, alignment padding, CRT heap overhead, and malloc users are excluded.
struct alignas(std::max_align_t) Header
{
    std::size_t requested;
    void *raw;
};

std::size_t g_liveBytes = 0;
std::size_t g_liveAllocations = 0;
std::size_t g_totalRequestedBytes = 0;
std::size_t g_totalAllocations = 0;
std::size_t g_highWaterBytes = 0;

void *allocate(std::size_t requested, std::size_t alignment)
{
    if (alignment < alignof(std::max_align_t))
        alignment = alignof(std::max_align_t);
    const std::size_t payload = requested == 0 ? 1 : requested;
    const std::size_t maximum = static_cast<std::size_t>(-1);
    if (alignment > maximum - sizeof(Header) || payload > maximum - sizeof(Header) - alignment)
        throw std::bad_alloc();
    void *raw = nullptr;
    while (!(raw = std::malloc(payload + sizeof(Header) + alignment - 1)))
    {
        const auto handler = std::get_new_handler();
        if (!handler)
            throw std::bad_alloc();
        handler();
    }
    const auto start = reinterpret_cast<std::uintptr_t>(raw) + sizeof(Header);
    const auto aligned = (start + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    auto *header = reinterpret_cast<Header *>(aligned) - 1;
    header->requested = requested;
    header->raw = raw;
    g_liveBytes += requested;
    ++g_liveAllocations;
    g_totalRequestedBytes += requested;
    ++g_totalAllocations;
    if (g_liveBytes > g_highWaterBytes)
        g_highWaterBytes = g_liveBytes;
    return reinterpret_cast<void *>(aligned);
}

void release(void *pointer) noexcept
{
    if (!pointer)
        return;
    auto *header = reinterpret_cast<Header *>(pointer) - 1;
    g_liveBytes -= header->requested;
    --g_liveAllocations;
    std::free(header->raw);
}
} // namespace

namespace allocations
{
Snapshot snapshot() noexcept
{
    return {g_liveBytes, g_liveAllocations, g_totalRequestedBytes, g_totalAllocations, g_highWaterBytes};
}

void resetPeak() noexcept { g_highWaterBytes = g_liveBytes; }
} // namespace allocations

void *operator new(std::size_t size) { return allocate(size, alignof(std::max_align_t)); }
void *operator new[](std::size_t size) { return allocate(size, alignof(std::max_align_t)); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    try
    {
        return allocate(size, alignof(std::max_align_t));
    }
    catch (...)
    {
        return nullptr;
    }
}
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    try
    {
        return allocate(size, alignof(std::max_align_t));
    }
    catch (...)
    {
        return nullptr;
    }
}
void *operator new(std::size_t size, std::align_val_t alignment)
{
    return allocate(size, static_cast<std::size_t>(alignment));
}
void *operator new[](std::size_t size, std::align_val_t alignment)
{
    return allocate(size, static_cast<std::size_t>(alignment));
}
void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    try
    {
        return allocate(size, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}
void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    try
    {
        return allocate(size, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}
void operator delete(void *pointer) noexcept { release(pointer); }
void operator delete[](void *pointer) noexcept { release(pointer); }
void operator delete(void *pointer, std::size_t) noexcept { release(pointer); }
void operator delete[](void *pointer, std::size_t) noexcept { release(pointer); }
void operator delete(void *pointer, const std::nothrow_t &) noexcept { release(pointer); }
void operator delete[](void *pointer, const std::nothrow_t &) noexcept { release(pointer); }
void operator delete(void *pointer, std::align_val_t) noexcept { release(pointer); }
void operator delete[](void *pointer, std::align_val_t) noexcept { release(pointer); }
void operator delete(void *pointer, std::size_t, std::align_val_t) noexcept { release(pointer); }
void operator delete[](void *pointer, std::size_t, std::align_val_t) noexcept { release(pointer); }
void operator delete(void *pointer, std::align_val_t, const std::nothrow_t &) noexcept { release(pointer); }
void operator delete[](void *pointer, std::align_val_t, const std::nothrow_t &) noexcept { release(pointer); }
