#include "Deserialize.h"

#include <new>
#include <stdexcept>

#include "kern/RuntimeError.h"

template <typename T> bool deserializeBasic(std::istream &stream, T &value)
{
  stream.read((char *)&value, sizeof(T));
  return stream.good();
}

bool deserialize(std::istream &stream, int8_t &value)
{
  return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, uint8_t &value)
{
  return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, int16_t &value)
{
  return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, uint16_t &value)
{
  return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, int32_t &value)
{
  return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, uint32_t &value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, uint64_t &value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, float &value)
{
  return deserializeBasic(stream, value);
}

bool deserialize(std::istream &stream, std::string &str, size_t maxLength)
{
  // Read length
  uint32_t length = 0;
  if (!deserialize(stream, length))
  {
    return false;
  }

  // A corrupt prefix must not reach reserve(), where it would throw bad_alloc
  // instead of a typed error.
  if (length > maxLength)
  {
    throw kern::RuntimeError("String length exceeds the limit.");
  }

  // TODO Empty string invalid?
  if (length == 0)
  {
    str.clear();
    return true;
  }

  // Reserve space. The limit check passes when the configured limit itself
  // is absurd, so a failing allocation must still surface as a typed error
  // instead of bad_alloc, matching the table-resize pattern.
  std::string temp;
  try
  {
    temp.reserve(length);
  }
  catch (const std::bad_alloc &)
  {
    throw kern::RuntimeError("String length exceeds the limit.");
  }
  catch (const std::length_error &)
  {
    throw kern::RuntimeError("String length exceeds the limit.");
  }

  // Read string content
  // TODO Slow
  for (uint32_t i = 0; i < length; ++i)
  {
    char c = 0;
    stream.read(&c, sizeof(c));
    // A truncated stream must fail cleanly instead of pushing an
    // indeterminate character and continuing.
    if (!stream.good())
    {
      return false;
    }
    temp.push_back(c);
  }

  // Move to actual storage
  str = std::move(temp);
  return stream.good();
}