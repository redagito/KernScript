#include "SExternFunction.h"

#include "Serialize.h"
#include "Deserialize.h"

bool serialize(const SExternFunction &externFunction, std::ostream &stream)
{
  // Sanity checks
  if (externFunction.name.empty())
  {
    return false;
  }

  // Write name
  uint32_t length = externFunction.name.length();
  stream.write((char *)&length, sizeof(length));
  stream.write((char *)externFunction.name.data(), length);

  // Write argument count
  stream.write((char *)&externFunction.argCount,
               sizeof(externFunction.argCount));

  // Return stream state
  return stream.good();
}

bool deserialize(std::istream &stream, SExternFunction &externFunction)
{
  // Function name
  std::string name;
  if (!deserialize(stream, name))
  {
    return false;
  }

  // Argument count
  uint8_t argCount;
  if (!deserialize(stream, argCount))
  {
    return false;
  }

  // Write data to actual target
  externFunction.name = std::move(name);
  externFunction.argCount = argCount;

  return true;
}