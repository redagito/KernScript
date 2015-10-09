#include <iostream>

#include "CVirtualMachine.h"

#include "common/Instructions.h"
#include "common/Deserialize.h"

CVirtualMachine::SFunctionFrame::SFunctionFrame() {}

CVirtualMachine::SFunctionFrame::SFunctionFrame(uint32_t funcIndex,
                                                uint32_t instrIndex,
                                                uint32_t stackBaseIndex)
{
  functionIndex = funcIndex;
  instructionIndex = instrIndex;
  runtimeStackBaseIndex = stackBaseIndex;
}

CVirtualMachine::CVirtualMachine()
{
  // Reserve runtime stack size.
  // TODO Remove hardcoded value
  m_runtimeStack.reserve(1024 * 256);
}

bool CVirtualMachine::load(std::istream &byteCode)
{
  // Load string constants
  if (!deserializeStrings(byteCode))
  {
    return false;
  }
  // Load extern functions
  if (!deserializeExternFunctions(byteCode))
  {
    return false;
  }
  // Load script functions
  if (!deserializeFunctions(byteCode))
  {
    return false;
  }
  return true;
}

void CVirtualMachine::clearScripts()
{
  m_currentFunctionIndex = 0;
  m_currentInstructionIndex = 0;
  m_currentRuntimeStackBaseIndex = 0;
  // Clear script data
  m_strings.clear();
  m_externFunctions.clear();
  m_functions.clear();
}

bool CVirtualMachine::callFunction(const std::string &functionName)
{
  // Retrieve function index if exists
  if (!getFunctionIndex(functionName, m_currentFunctionIndex))
  {
    return false;
  }
  m_currentInstructionIndex = 0;

  // Execute function
  bool running = true;
  while (running)
  {
    // Single step execution until finished
    running = execute();
  }
  return true;
}

void CVirtualMachine::addFunction(const std::string &functionName,
                                  kern::IExternFunction *function)
{
  m_externFunctionsMap[functionName].reset(function);
}

void CVirtualMachine::removeFunction(const ::std::string &functionName)
{
  m_externFunctionsMap.erase(functionName);
}

bool CVirtualMachine::popParameter(int &value)
{
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool CVirtualMachine::popParameter(float &value)
{
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool CVirtualMachine::popParameter(::std::string &value)
{
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

void CVirtualMachine::setReturnValue() { m_runtimeStack.push_back(CValue(0)); }

void CVirtualMachine::setReturnValue(int value)
{
  m_runtimeStack.push_back(CValue(value));
}

void CVirtualMachine::setReturnValue(float value)
{
  m_runtimeStack.push_back(CValue(value));
}

void CVirtualMachine::setReturnValue(const std::string &value)
{
  m_runtimeStack.push_back(CValue(value));
}

bool CVirtualMachine::deserializeStrings(std::istream &stream)
{
  // Get string table size and reserve space
  uint32_t size = 0;
  if (!deserialize(stream, size))
  {
    return false;
  }
  m_strings.resize(size);

  // Read strings into string table
  for (auto &str : m_strings)
  {
    if (!deserialize(stream, str))
    {
      return false;
    }
  }
  return true;
}

bool CVirtualMachine::deserializeExternFunctions(std::istream &stream)
{
  // Read size
  uint32_t size = 0;
  stream.read((char *)&size, sizeof(size));
  m_externFunctions.resize(size);
  for (auto &externFunction : m_externFunctions)
  {
    if (!deserialize(stream, externFunction))
    {
      return false;
    }
  }
  return stream.good();
}

bool CVirtualMachine::deserializeFunctions(std::istream &stream)
{
  // Read size
  uint32_t size = 0;
  stream.read((char *)&size, sizeof(size));
  // Reserve space
  m_functions.resize(size);

  // Read function data
  for (auto &function : m_functions)
  {
    // Read function name
    uint32_t length = 0;
    stream.read((char *)&length, sizeof(length));
    function.name.reserve(length);
    for (uint32_t i = 0; i < length; ++i)
    {
      char c;
      stream.read(&c, 1);
      function.name.push_back(c);
    }

    // Read stack size
    stream.read((char *)&function.stackSize, sizeof(function.stackSize));

    // Read instructions
    // Instructions size
    stream.read((char *)&size, sizeof(size));
    function.instructions.resize(size);
    // Instructions
    for (auto &instruction : function.instructions)
    {
      if (!deserialize(instruction, stream))
      {
        return false;
      }
    }
  }

  return stream.good();
}

bool CVirtualMachine::getFunctionIndex(const std::string &functionName,
                                       uint32_t &index)
{
  index = 0;
  for (const auto &function : m_functions)
  {
    if (function.name == functionName)
    {
      return true;
    }
    ++index;
  }
  return false;
}

bool CVirtualMachine::execute()
{
  SInstruction instruction;
  // Check for out of bounds instruction index
  if (m_functions.at(m_currentFunctionIndex).instructions.size() <=
      m_currentInstructionIndex)
  {
    // Implicit return
    instruction.id = EInstruction::Ret;
  }
  else
  {
    // Retrieve current instruction
    instruction = m_functions.at(m_currentFunctionIndex)
                      .instructions.at(m_currentInstructionIndex);
  }

  switch (instruction.id)
  {
  case EInstruction::Nop:
    ++m_currentInstructionIndex;
    break;
  case EInstruction::Break:
    // Not impelmented
    ++m_currentInstructionIndex;
    break;
  case EInstruction::Exit:
    // Return false to signal end of script
    return false;
    break;

  case EInstruction::Movv:
    // Move variable to variable
    // Arg 0 is variable index of destination
    // Arg 1 is variable index of source
    m_runtimeStack[m_currentRuntimeStackBaseIndex +
                   *((uint32_t *)&instruction.args[0])] =
        m_runtimeStack.at(m_currentRuntimeStackBaseIndex +
                          *((uint32_t *)&instruction.args[1]));
    break;
  case EInstruction::Movi:
    // Move integer to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source integer value
    m_runtimeStack[m_currentRuntimeStackBaseIndex +
                   *((uint32_t *)&instruction.args[0])] = instruction.args[1];
    break;
  case EInstruction::Movf:
    // Move float to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source float value
    m_runtimeStack[m_currentRuntimeStackBaseIndex +
                   *((uint32_t *)&instruction.args[0])] =
        *((float *)&instruction.args[1]);
    break;

  case EInstruction::Pushi:
    // Arg 0 is integer value
    m_runtimeStack.push_back(CValue(instruction.args[0]));
    ++m_currentInstructionIndex;
    break;
  case EInstruction::Pushf:
    // Arg 0 is float value
    m_runtimeStack.push_back(CValue(*((float *)&instruction.args[0])));
    ++m_currentInstructionIndex;
    break;
  case EInstruction::Pop:
    // Remove top element from runtime stack
    m_runtimeStack.pop_back();
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Call:
    // Push next instruction and current function index and base stack index for
    // return call
    m_callStack.push(SFunctionFrame(m_currentFunctionIndex,
                                    m_currentInstructionIndex + 1,
                                    m_currentRuntimeStackBaseIndex));
    // Arg 0 is function index
    m_currentFunctionIndex = *((uint32_t *)&instruction.args[0]);
    // Reset instruction index
    m_currentInstructionIndex = 0;
    // Set new runtime stack base index for the called function
    m_currentRuntimeStackBaseIndex = m_runtimeStack.size();
    // Resize runtime stack with local stack size of called function
    m_runtimeStack.resize(m_runtimeStack.size() +
                          m_functions.at(m_currentFunctionIndex).stackSize);
    break;
  case EInstruction::Calle:
  {
    // Call external, arg 0 is extern function index
    const SExternFunction &externFunction =
        m_externFunctions.at(instruction.args[0]);

    // Check if function exists
    // TODO Check if arg count ok
    auto entry = m_externFunctionsMap.find(externFunction.name);
    if (entry == m_externFunctionsMap.end())
    {
      // Function does not exist
      std::cout << "The extern function '" << externFunction.name
                << "' does not exist." << std::endl;
      return false;
    }
    // Call if found
    entry->second->call(*this);
    ++m_currentInstructionIndex;
    break;
  }
  case EInstruction::Ret:
    // Return from script function
    if (m_callStack.empty())
    {
      // Empty stack indicates either error state or end of script
      return false;
    }
    // Resize runtime stack to remove local function variables but one.
    // The variable and local stack index 0 holds the return value and is not
    // cleared.
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  case EInstruction::Add:
  {
    CValue x;
    // 2 Values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      return false;
    }
    m_runtimeStack.back() += x;
    ++m_currentInstructionIndex;
  }
  break;
  case EInstruction::Sub:
  {
    CValue x;
    // 2 Values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      return false;
    }
    m_runtimeStack.back() -= x;
    ++m_currentInstructionIndex;
  }
  break;
  case EInstruction::Mul:
    // Not implemented
    return false;
    break;
  case EInstruction::Div:
    // Not implemented
    return false;
    break;
  case EInstruction::Inc:
    // Not implemented
    return false;
    break;
  case EInstruction::Dec:
    // Not implemented
    return false;
    break;

  case EInstruction::And:
    // Not implemented
    return false;
    break;
  case EInstruction::Or:
    // Not implemented
    return false;
    break;
  case EInstruction::Not:
    // Not implemented
    return false;
    break;
  case EInstruction::Xor:
    // Not implemented
    return false;
    break;

  case EInstruction::Jmp:
    // Not implemented
    return false;
    break;
  case EInstruction::Je:
  {
    // Compare top 2 values from stack, x == y
    CValue y;
    CValue x;
    if (!popValue(y) || !popValue(x))
    {
      // Not enough values on stack
      return false;
    }
    if (x == y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = *((uint32_t *)&instruction.args[0]);
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  case EInstruction::Jne:
  {
    // Compare top 2 values from stack, x != y
    CValue y;
    CValue x;
    if (!popValue(y) || !popValue(x))
    {
      // Not enough values on stack
      return false;
    }
    if (x != y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = *((uint32_t *)&instruction.args[0]);
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  case EInstruction::Jle:
  {
    // Compare top 2 values from stack, x <= y
    CValue y;
    CValue x;
    if (!popValue(y) || !popValue(x))
    {
      // Not enough values on stack
      return false;
    }
    if (x <= y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = *((uint32_t *)&instruction.args[0]);
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  }

  return true;
}

void CVirtualMachine::pushValue(const CValue &val)
{
  m_runtimeStack.push_back(val);
}

bool CVirtualMachine::popValue(CValue &val)
{
  if (m_runtimeStack.empty())
  {
    return false;
  }
  val = std::move(m_runtimeStack.back());
  m_runtimeStack.pop_back();
  return true;
}