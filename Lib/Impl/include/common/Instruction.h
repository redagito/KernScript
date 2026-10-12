#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>

#include "common/Instructions.h"

typedef int32_t ArgumentType;

/**
 * \brief Represents an assembled instruction.
 */
struct Instruction
{
    Opcode id = Opcode::Nop; /**< Instruction id. */
    // Zero-initialized: instructions with fewer operands leave the remaining
    // slots untouched, and unset slots must not carry garbage.
    ArgumentType args[4] = {}; /**< Max 4 arguments supported per instruction. */
};

bool serialize(const Instruction &instruction, std::ostream &stream);
bool deserialize(Instruction &instruction, std::istream &stream);

std::string toString(const Instruction &instruction);
