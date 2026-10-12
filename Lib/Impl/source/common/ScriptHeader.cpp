#include "common/ScriptHeader.h"

#include "common/Deserialize.h"

bool serialize(const ScriptHeader &header, std::ostream &stream)
{
    // 4 byte identifier
    stream.write((char *)header.identifier, sizeof(header.identifier));
    // 1 byte major version
    stream.write((char *)&header.majorVersion, sizeof(header.majorVersion));
    // 1 byte minor version
    stream.write((char *)&header.minorVersion, sizeof(header.minorVersion));
    // Module name as a length prefixed string, so load() knows where the
    // string table starts.
    uint32_t length = static_cast<uint32_t>(header.moduleName.length());
    stream.write((char *)&length, sizeof(length));
    stream.write(header.moduleName.data(), length);
    return stream.good();
}

bool deserialize(ScriptHeader &header, std::istream &stream, size_t maxModuleLength)
{
    // 4 byte identifier
    stream.read((char *)header.identifier, sizeof(header.identifier));
    // 1 byte major version
    stream.read((char *)&header.majorVersion, sizeof(header.majorVersion));
    // 1 byte minor version
    stream.read((char *)&header.minorVersion, sizeof(header.minorVersion));
    if (!stream.good())
    {
        return false;
    }
    std::string moduleName;
    if (!deserialize(stream, moduleName, maxModuleLength))
    {
        return false;
    }
    header.moduleName = std::move(moduleName);
    return true;
}