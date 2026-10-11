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
*/
struct VMConfig
{
  size_t maxStackBytes = 1024 * 1024;
  uint32_t maxCallDepth = 1024;
};

}
