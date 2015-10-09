#include "SScriptHeader.h"

bool serialize(const SScriptHeader &header, std::ostream &stream)
{
  // 4 byte identifier
  stream.write((char *)header.identifier, sizeof(header.identifier));
  // 1 byte major version
  stream.write((char *)&header.majorVersion, sizeof(header.majorVersion));
  // 1 byte minor version
  stream.write((char *)&header.minorVersion, sizeof(header.minorVersion));
  return stream.good();
}

bool deserialize(SScriptHeader &header, std::istream &stream)
{
  // 4 byte identifier
  stream.read((char *)header.identifier, sizeof(header.identifier));
  // 1 byte major version
  stream.read((char *)&header.majorVersion, sizeof(header.majorVersion));
  // 1 byte minor version
  stream.read((char *)&header.minorVersion, sizeof(header.minorVersion));
  return stream.good();
}