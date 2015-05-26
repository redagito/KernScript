#include "SInstruction.h"

bool serialize(const SInstruction& instruction, std::ostream& stream)
{
	// Write id
	uint16_t id = (uint16_t)instruction.id;
	stream.write((char*) &id, sizeof(id));
	// Number of arguments depends on instruction id
	for (unsigned int i = 0; i < instructions[id].argCount; ++i)
	{
		// Write argument
		stream.write((char*) &instruction.args[i], sizeof(ArgumentType));
	}
	return stream.good();
}

bool deserialize(SInstruction& instruction, std::istream& stream)
{
	// Read id
	uint16_t id;
	stream.read((char*) &id, sizeof(id));
	instruction.id = (EInstruction) id;
	// Number of arguments depends on instruction id
	for (unsigned int i = 0; i < instructions[id].argCount; ++i)
	{
		// Read argument
		stream.read((char*) &instruction.args[i], sizeof(ArgumentType));
	}
	return stream.good();
}