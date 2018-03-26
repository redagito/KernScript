#pragma once

#include <cstdint>
#include <istream>
#include <ostream>

/**
* \brief Represents an assembled script header.
*/
struct SScriptHeader
{
  uint8_t
      identifier[4]; /**< Contains identifier for script validation. Should be
                        set to KERN. */
  uint8_t majorVersion;   /**< Contains major release version. */
  uint8_t minorVersion;   /**< Contains minor release version. */
  std::string moduleName; /**< Script module name identifier. */
};

bool serialize(const SScriptHeader &header, std::ostream &stream);
bool deserialize(SScriptHeader &header, std::istream &stream);