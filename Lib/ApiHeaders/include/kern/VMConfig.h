#pragma once

#include <cstddef>
#include <cstdint>

namespace kern
{

/**
 * \brief Limits for a virtual machine instance.
 *
 * maxStackBytes bounds the runtime stack. It is converted to slots by dividing
 * by the value size, so a zero budget allows nothing at all. maxCallDepth
 * bounds nested script calls separately, because a function without parameters
 * or locals grows the call stack while the runtime stack stays flat.
 * maxStringBytes bounds a single string or name read from byte code, and
 * maxTableEntries bounds each byte code table, so corrupt length prefixes
 * throw kern::RuntimeError instead of allocating unbounded individual tables.
 * maxProgramBytes additionally bounds their combined storage and contents.
 * maxRuntimeStringBytes bounds live host-returned string contents; copies
 * share immutable storage and loaded constants use the program budget.
 * maxExecutionSteps bounds the instructions one callFunction runs, so a
 * backward jump to itself throws kern::RuntimeError instead of hanging the
 * host. The count resets on every callFunction.
 */
struct VMConfig
{
    size_t maxStackBytes = 1024 * 1024;
    uint32_t maxCallDepth = 1024;
    size_t maxStringBytes = 1024 * 1024;
    uint32_t maxTableEntries = 1024 * 1024;
    uint64_t maxExecutionSteps = 10000000;
    // Aggregate table storage, instruction storage and string/name contents.
    // Allocator overhead and temporary decoding buffers are not included.
    size_t maxProgramBytes = 64 * 1024 * 1024;
    // Live host-returned string contents; immutable copies share one charge.
    // Loaded string constants are covered by maxProgramBytes instead.
    size_t maxRuntimeStringBytes = 16 * 1024 * 1024;
};

} // namespace kern
