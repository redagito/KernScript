#include "SExternFunction.h"

bool serialize(const SExternFunction& externFunction, std::ostream& stream)
{
	// Sanity checks
	if (externFunction.name.empty())
	{
		return false;
	}

	// Write name
	uint32_t length = externFunction.name.length();
	stream.write((char*) &length, sizeof(length));
	stream.write((char*) externFunction.name.data(), length);

	// Write argument count
	stream.write((char*) &externFunction.argCount, sizeof(externFunction.argCount));

	// Return stream state	
	return stream.good();
}

bool deserialize(SExternFunction& externFunction, std::istream& stream)
{
	// Read function name
	uint32_t length = 0;
	stream.read((char*) &length, 4);
	if (length == 0)
	{
		return false;
	}
	// TODO Clear ok?
	externFunction.name = ""; // Set to empty
	externFunction.name.reserve(length);
	// TODO Slow? Can be done better
	for (uint32_t i = 0; i < length; ++i)
	{
		char c;
		stream.read(&c, 1);
		externFunction.name.push_back(c);
	}

	// Read function arguments
	stream.read((char*) &externFunction.argCount, sizeof(externFunction.argCount));
	return stream.good();
}