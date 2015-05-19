#pragma once

#include <cstdint>

#include "Instructions.h"

typedef int32_t ArgumentType;

/**
* \brief Represents an assembled instruction.
*/
struct SInstruction
{
	InstructionId id = instrNop; /**< Instruction id. */
	ArgumentType args[4]; /**< Max 4 arguments supported per instruction. */
};