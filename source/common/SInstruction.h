#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>

#include "Instructions.h"

typedef int32_t ArgumentType;

/**
* \brief Represents an assembled instruction.
*/
struct SInstruction
{
  EInstruction id = EInstruction::Nop; /**< Instruction id. */
  ArgumentType args[4]; /**< Max 4 arguments supported per instruction. */
};

bool serialize(const SInstruction &instruction, std::ostream &stream);
bool deserialize(SInstruction &instruction, std::istream &stream);

std::string toString(const SInstruction &instruction);
