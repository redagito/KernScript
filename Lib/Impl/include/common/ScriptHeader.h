#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>

/**
 * \brief Represents an assembled script header.
 */
struct ScriptHeader {
  uint8_t identifier[4];  /**< Contains identifier for script validation. Should be
                             set to KERN. */
  uint8_t majorVersion;   /**< Contains major release version. */
  uint8_t minorVersion;   /**< Contains minor release version. */
  std::string moduleName; /**< Script module name identifier. */
};

bool serialize(const ScriptHeader &header, std::ostream &stream);

/**
 * \brief Deserializes a script header, bounding the module name allocation.
 */
bool deserialize(ScriptHeader &header, std::istream &stream, size_t maxModuleLength);