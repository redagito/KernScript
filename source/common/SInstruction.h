#pragma once

#include <cstdint>
#include <istream>
#include <ostream>

#include "Instructions.h"

typedef int32_t ArgumentType;

/**
* \brief Represents an assembled instruction.
*/
struct SInstruction
{
	EInstructíon id = EInstructíon::Nop; /**< Instruction id. */
	ArgumentType args[4]; /**< Max 4 arguments supported per instruction. */
};

bool serialize(const SInstruction& instruction, std::ostream& stream);
bool deserialize(SInstruction& instruction, std::istream& stream);