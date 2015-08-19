#include "Deserialize.h"

template <typename T>
bool deserializeBasic(std::istream& stream, T& value)
{
	stream.read((char*) &value, sizeof(T));
	return stream.good();
}

bool deserialize(std::istream& stream, int8_t& value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream& stream, uint8_t& value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream& stream, int16_t& value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream& stream, uint16_t& value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream& stream, int32_t& value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream& stream, uint32_t& value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream& stream, float& value)
{
	return deserializeBasic(stream, value);
}

bool deserialize(std::istream& stream, std::string& str)
{
	// Read length
	uint32_t length = 0;
	if (!deserialize(stream, length)) { return false; }
	
	// TODO Empty string invalid?
	if (length == 0) { return true; }
	
	// Reserve space
	std::string temp;		
	temp.reserve(length);
	
	// Read string content
	for (uint32_t i = 0; i < length; ++i)
	{
		// TODO Slow?
		char c;
		stream.read(&c, 1);
		temp.push_back(c);
	}
	
	// Move to actual storage
	str = std::move(temp);
	return stream.good();
}