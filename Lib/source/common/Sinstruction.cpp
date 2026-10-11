#include "SInstruction.h"

namespace
{

/**
* \brief Number of entries in the instruction lookup table.
*/
unsigned int instructionCount()
{
  return static_cast<unsigned int>(sizeof(instructions) /
                                   sizeof(instructions[0]));
}

/**
* \brief Checks an instruction id read from untrusted input.
*/
bool validId(uint16_t id)
{
  return id < instructionCount();
}

}

bool serialize(const SInstruction &instruction, std::ostream &stream)
{
  const uint16_t id = static_cast<uint16_t>(instruction.id);
  if (!validId(id))
  {
    return false;
  }
  // Write id
  stream.write((char *)&id, sizeof(id));
  // Number of arguments depends on instruction id
  for (unsigned int i = 0; i < instructions[id].argCount; ++i)
  {
    // Write argument
    stream.write((char *)&instruction.args[i], sizeof(ArgumentType));
  }
  return stream.good();
}

bool deserialize(SInstruction &instruction, std::istream &stream)
{
  // Read id
  uint16_t id = 0;
  stream.read((char *)&id, sizeof(id));
  if (!stream.good() || !validId(id))
  {
    return false;
  }
  instruction.id = (EInstruction)id;
  // Number of arguments depends on instruction id
  for (unsigned int i = 0; i < instructions[id].argCount; ++i)
  {
    // Read argument
    stream.read((char *)&instruction.args[i], sizeof(ArgumentType));
  }
  return stream.good();
}

std::string toString(const SInstruction &instruction)
{
  const uint16_t id = static_cast<uint16_t>(instruction.id);
  if (!validId(id))
  {
    return "<invalid>";
  }
  return instructions[id].mnemonic;
}
