#include "SInstruction.h"

bool serialize(const SInstruction& instruction, std::ostream& stream)
{
	// Write id
	stream.write((char*) &instruction.id, sizeof(instruction.id));
	// Number of arguments depends on instruction id
	for (unsigned int i = 0; i < instructions[instruction.id].argCount; ++i)
	{
		// Write argument
		stream.write((char*) &instruction.args[i], sizeof(ArgumentType));
	}
	return stream.good();
}

bool deserialize(SInstruction& instruction, std::istream& stream)
{
	// Read id
	stream.read((char*) &instruction.id, sizeof(instruction.id));
	// Number of arguments depends on instruction id
	for (unsigned int i = 0; i < instructions[instruction.id].argCount; ++i)
	{
		// Read argument
		stream.read((char*) &instruction.args[i], sizeof(ArgumentType));
	}
	return stream.good();
}