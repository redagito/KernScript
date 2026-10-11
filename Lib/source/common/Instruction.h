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
struct Instruction
{
  Opcode id = Opcode::Nop; /**< Instruction id. */
  ArgumentType args[4]; /**< Max 4 arguments supported per instruction. */
};

bool serialize(const Instruction &instruction, std::ostream &stream);
bool deserialize(Instruction &instruction, std::istream &stream);

std::string toString(const Instruction &instruction);
