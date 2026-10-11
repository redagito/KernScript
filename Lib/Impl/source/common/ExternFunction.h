#pragma once

#include <string>
#include <cstdint>
#include <istream>
#include <ostream>

/**
* \brief Represents an assembled extern function.
*/
struct ExternFunction
{
  std::string name;     /**< Extern function name. */
  uint8_t argCount = 0; /**< Expected argument count. */
};

/**
* \brief Serializes extern function to stream.
*/
bool serialize(const ExternFunction &externFunction, std::ostream &stream);

/**
* \brief Deserializes extern function from stream.
*/
bool deserialize(std::istream &stream, ExternFunction &externFunction);