#include "common/Instruction.h"
std::string toString(const Instruction &instruction)
{
    const auto raw = static_cast<size_t>(instruction.id);
    return raw < std::size(instructions) ? instructions[raw].mnemonic : "<invalid>";
}
