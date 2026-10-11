#include <cassert>
#include <cstring>

#include "VirtualMachine.h"

#include "kern/RuntimeError.h"

#include "common/Instructions.h"
#include "common/Deserialize.h"
#include "logging/Logger.h"

namespace
{

/**
* \brief Logs a runtime error and throws it.
*
* Every error path in execute() goes through here, so diagnostics keep flowing
* to the logger while the failure itself is a typed exception that tests and
* hosts assert on instead of matching message text.
*/
template <typename... Args>
[[noreturn]] void throwRuntimeError(kern::ILogger *logger,
                                    fmt::format_string<Args...> format,
                                    Args &&...args)
{
  std::string message = fmt::format(format, std::forward<Args>(args)...);
  logger->error("{}", message);
  throw kern::RuntimeError(message);
}

/**
* \brief Checks an index read from bytecode against a container size.
*
* Indices arrive as signed 32 bit values. Reinterpreting a negative one as
* unsigned would wrap, so the addition runs in 64 bit where nothing wraps.
*/
bool indexInRange(uint32_t base, int32_t rawIndex, size_t size)
{
  const uint64_t index =
      static_cast<uint64_t>(base) + static_cast<uint64_t>(static_cast<uint32_t>(rawIndex));
  return index < static_cast<uint64_t>(size);
}

}

VirtualMachine::FunctionFrame::FunctionFrame() {}

VirtualMachine::FunctionFrame::FunctionFrame(uint32_t funcIndex,
                                                 uint32_t instrIndex,
                                                 uint32_t stackBaseIndex)
{
  functionIndex = funcIndex;
  instructionIndex = instrIndex;
  runtimeStackBaseIndex = stackBaseIndex;
}

VirtualMachine::VirtualMachine(kern::ILogger *logger)
    : VirtualMachine(logger, kern::VMConfig{})
{
}

VirtualMachine::VirtualMachine(kern::ILogger *logger, kern::VMConfig config)
    : m_logger(logger != nullptr ? logger : nullLogger()),
      m_maxStackSize(config.maxStackBytes / sizeof(Value)),
      m_maxCallDepth(config.maxCallDepth),
      m_maxStringBytes(config.maxStringBytes),
      m_maxTableEntries(config.maxTableEntries)
{
  // Preallocate the whole budget, so steady state never reallocates.
  m_runtimeStack.reserve(m_maxStackSize);
}

bool VirtualMachine::load(std::istream &byteCode)
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

void VirtualMachine::clearScripts()
{
  m_currentFunctionIndex = 0;
  m_currentInstructionIndex = 0;
  m_currentRuntimeStackBaseIndex = 0;
  // Clear script data
  m_strings.clear();
  m_externFunctions.clear();
  m_functions.clear();
  // Drop any values and frames a previous call left behind.
  m_runtimeStack.clear();
  while (!m_callStack.empty())
  {
    m_callStack.pop();
  }
}

bool VirtualMachine::callFunction(const std::string &functionName)
{
  // Retrieve function index if exists
  if (!getFunctionIndexByName(functionName, m_currentFunctionIndex))
  {
    return false;
  }
  // Start from a clean slate: a previous call may have thrown out of nested
  // frames, leaving stale values, frames and a base index behind. Clearing
  // first also keeps locals from leaking values a previous call left in the
  // reused slots; the resize below then value-initializes the whole frame.
  m_runtimeStack.clear();
  while (!m_callStack.empty())
  {
    m_callStack.pop();
  }
  m_currentRuntimeStackBaseIndex = 0;
  m_currentInstructionIndex = 0;
  const uint32_t stackSize =
      m_functions.at(m_currentFunctionIndex).stackSize;
  if (stackSize > m_maxStackSize)
  {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
  }
  m_runtimeStack.resize(stackSize);

  // Execute function
  bool running = true;
  while (running)
  {
    // Single step execution until finished
    running = execute();
  }
  return true;
}

void VirtualMachine::addFunction(const std::string &functionName,
                                  kern::IExternFunction *function)
{
  m_externFunctionsMap[functionName].reset(function);
}

void VirtualMachine::removeFunction(const ::std::string &functionName)
{
  m_externFunctionsMap.erase(functionName);
}

bool VirtualMachine::popParameter(int &value)
{
  if (m_runtimeStack.empty())
  {
    throwRuntimeError(m_logger,
                      "popParameter needs a value on the runtime stack.");
  }
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool VirtualMachine::popParameter(float &value)
{
  if (m_runtimeStack.empty())
  {
    throwRuntimeError(m_logger,
                      "popParameter needs a value on the runtime stack.");
  }
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool VirtualMachine::popParameter(::std::string &value)
{
  if (m_runtimeStack.empty())
  {
    throwRuntimeError(m_logger,
                      "popParameter needs a value on the runtime stack.");
  }
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

void VirtualMachine::setReturnValue() { return; }

void VirtualMachine::setReturnValue(int value) { pushValue(Value(value)); }

void VirtualMachine::setReturnValue(float value) { pushValue(Value(value)); }

void VirtualMachine::setReturnValue(const std::string &value)
{
  pushValue(Value(value));
}

bool VirtualMachine::deserializeStrings(std::istream &stream)
{
  // Get string table size and reserve space
  uint32_t size = 0;
  if (!deserialize(stream, size))
  {
    return false;
  }
  // A corrupt count must not reach resize(), where it would throw bad_alloc
  // instead of a typed error.
  if (size > m_maxTableEntries)
  {
    throwRuntimeError(m_logger, "String table size {} exceeds the limit.",
                      size);
  }
  m_strings.resize(size);

  // Read strings into string table
  for (auto &str : m_strings)
  {
    if (!deserialize(stream, str, m_maxStringBytes))
    {
      return false;
    }
  }
  return true;
}

bool VirtualMachine::deserializeExternFunctions(std::istream &stream)
{
  // Read size
  uint32_t size = 0;
  stream.read((char *)&size, sizeof(size));
  if (size > m_maxTableEntries)
  {
    throwRuntimeError(m_logger, "Extern function count {} exceeds the limit.",
                      size);
  }
  m_externFunctions.resize(size);
  for (auto &externFunction : m_externFunctions)
  {
    if (!deserialize(stream, externFunction, m_maxStringBytes))
    {
      return false;
    }
  }
  return stream.good();
}

bool VirtualMachine::deserializeFunctions(std::istream &stream)
{
  // Read size
  uint32_t size = 0;
  stream.read((char *)&size, sizeof(size));
  if (size > m_maxTableEntries)
  {
    throwRuntimeError(m_logger, "Function count {} exceeds the limit.", size);
  }
  // Reserve space
  m_functions.resize(size);

  // Read function data
  for (auto &function : m_functions)
  {
    // Read function name
    uint32_t length = 0;
    stream.read((char *)&length, sizeof(length));
    if (length > m_maxStringBytes)
    {
      throwRuntimeError(m_logger, "Function name length {} exceeds the limit.",
                        length);
    }
    function.name.reserve(length);
    for (uint32_t i = 0; i < length; ++i)
    {
      char c;
      stream.read(&c, 1);
      function.name.push_back(c);
    }

    // Read stack size
    stream.read((char *)&function.stackSize, sizeof(function.stackSize));
    // Read parameter size
    stream.read((char *)&function.parameterSize,
                sizeof(function.parameterSize));

    // Read instructions
    // Instructions size
    stream.read((char *)&size, sizeof(size));
    if (size > m_maxTableEntries)
    {
      throwRuntimeError(m_logger, "Instruction count {} exceeds the limit.",
                        size);
    }
    function.instructions.resize(size);
    // Instructions
    for (auto &instruction : function.instructions)
    {
      if (!deserialize(instruction, stream))
      {
        return false;
      }
    }
    // Cache size for fast lookup
    function.instructionSize = size;
  }
  return stream.good();
}

bool VirtualMachine::getFunctionIndexByName(const std::string &functionName,
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

bool VirtualMachine::execute()
{
  // Cache current function
  const Function &currentFunction = m_functions.at(m_currentFunctionIndex);

  // Default current instruction to implicit return
  Instruction instruction;
  instruction.id = Opcode::Ret;

  // Check for out of bounds instruction index
  if (currentFunction.instructionSize > m_currentInstructionIndex)
  {
    // Retrieve current instruction
    instruction = currentFunction.instructions.at(m_currentInstructionIndex);
  }

  switch (instruction.id)
  {
  case Opcode::Nop:
    ++m_currentInstructionIndex;
    break;

  case Opcode::Break:
    // Breakpoint, for debugging. There is no debugger attached, so this
    // intentionally does nothing and execution continues.
    ++m_currentInstructionIndex;
    break;

  case Opcode::Exit:
    // Return false to signal end of script
    return false;
    break;

  case Opcode::Movv:
  {
    // Move variable to variable
    // Arg 0 is variable index of destination
    // Arg 1 is variable index of source
    if (!indexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0],
                      m_runtimeStack.size()) ||
        !indexInRange(m_currentRuntimeStackBaseIndex, instruction.args[1],
                      m_runtimeStack.size()))
    {
      throwRuntimeError(m_logger, "Movv variable index out of range.");
    }
    // Args hold raw bits, so copy them out instead of punning the pointer.
    uint32_t destIndex = 0;
    uint32_t srcIndex = 0;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    memcpy(&srcIndex, &instruction.args[1], sizeof(srcIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] =
        m_runtimeStack.at(m_currentRuntimeStackBaseIndex + srcIndex);
    ++m_currentInstructionIndex;
    break;
  }

  case Opcode::Movi:
  {
    // Move integer to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source integer value
    if (!indexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0],
                      m_runtimeStack.size()))
    {
      throwRuntimeError(m_logger, "Movi variable index out of range.");
    }
    uint32_t destIndex = 0;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] =
        instruction.args[1];
    ++m_currentInstructionIndex;
    break;
  }

  case Opcode::Movf:
  {
    // Move float to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source float value
    if (!indexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0],
                      m_runtimeStack.size()))
    {
      throwRuntimeError(m_logger, "Movf variable index out of range.");
    }
    uint32_t destIndex = 0;
    float value = 0.f;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    memcpy(&value, &instruction.args[1], sizeof(value));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] = value;
    ++m_currentInstructionIndex;
    break;
  }

  case Opcode::Movs:
  {
    // Move string to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source index of string
    if (!indexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0],
                      m_runtimeStack.size()))
    {
      throwRuntimeError(m_logger, "Movs variable index out of range.");
    }
    if (!indexInRange(0, instruction.args[1], m_strings.size()))
    {
      throwRuntimeError(m_logger, "Movs string index out of range.");
    }
    uint32_t destIndex = 0;
    uint32_t stringIndex = 0;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    memcpy(&stringIndex, &instruction.args[1], sizeof(stringIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] =
        m_strings.at(stringIndex);
    ++m_currentInstructionIndex;
    break;
  }

  case Opcode::Pushi:
    // Push signed 32 bit integer value to
    // Arg 0 is integer value
    pushValue(Value(instruction.args[0]));
    ++m_currentInstructionIndex;
    break;

  case Opcode::Pushu:
  {
    // Arg 0 is an unsigned integer value
    uint32_t value = 0;
    memcpy(&value, &instruction.args[0], sizeof(value));
    pushValue(Value(value));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Pushs:
  {
    // Arg 0 is string index
    uint32_t stringIndex = 0;
    memcpy(&stringIndex, &instruction.args[0], sizeof(stringIndex));
    if (!indexInRange(0, instruction.args[0], m_strings.size()))
    {
      throwRuntimeError(m_logger, "Pushs string index out of range.");
    }
    pushValue(m_strings.at(stringIndex));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Pushf:
  {
    // Arg 0 is float value
    float value = 0.f;
    memcpy(&value, &instruction.args[0], sizeof(value));
    pushValue(Value(value));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Pushv:
  {
    // Arg 0 is variable index
    uint32_t varIndex = 0;
    memcpy(&varIndex, &instruction.args[0], sizeof(varIndex));
    if (!indexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0],
                      m_runtimeStack.size()))
    {
      throwRuntimeError(m_logger, "Pushv variable index out of range.");
    }
    pushValue(m_runtimeStack.at(m_currentRuntimeStackBaseIndex + varIndex));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Pop:
    // Remove top element from runtime stack
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Pop needs a value on the runtime stack.");
    }
    m_runtimeStack.pop_back();
    ++m_currentInstructionIndex;
    break;

  case Opcode::Popv:
  {
    // Remove top of the stack and store it in variable
    // Arg 0 is variable index
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Popv needs a value on the runtime stack.");
    }
    if (!indexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0],
                      m_runtimeStack.size()))
    {
      throwRuntimeError(m_logger, "Popv variable index out of range.");
    }
    uint32_t destIndex = 0;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] =
        m_runtimeStack.back();
    m_runtimeStack.pop_back();
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Call:
  {
    // Arg 0 is function index of the called function
    const int32_t functionIndex = instruction.args[0];
    if (functionIndex < 0 ||
        static_cast<uint64_t>(functionIndex) >= m_functions.size())
    {
      throwRuntimeError(m_logger, "Call function index out of range.");
    }
    const Function &newCurrentFunction =
        m_functions.at(static_cast<size_t>(functionIndex));
    const uint32_t runtimeStackSize =
        static_cast<uint32_t>(m_runtimeStack.size());
    if (newCurrentFunction.parameterSize > runtimeStackSize)
    {
      throwRuntimeError(m_logger,
                        "Call needs {} parameters on the runtime stack.",
                        newCurrentFunction.parameterSize);
    }
    if (m_callStack.size() >= m_maxCallDepth)
    {
      throwRuntimeError(m_logger, "Maximum call depth exceeded.");
    }
    // All arithmetic below runs in 64 bit: 32 bit stack sizes from corrupt
    // input wrap around and would otherwise resize the stack small.
    const uint64_t newSize =
        static_cast<uint64_t>(runtimeStackSize) +
        static_cast<uint64_t>(newCurrentFunction.stackSize) -
        static_cast<uint64_t>(newCurrentFunction.parameterSize);
    if (newSize > m_maxStackSize)
    {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    // Push current function index, next instruction index and base stack index
    // for
    // return call.
    m_callStack.push(FunctionFrame(m_currentFunctionIndex,
                                     m_currentInstructionIndex + 1,
                                     m_currentRuntimeStackBaseIndex));
    m_currentFunctionIndex = static_cast<uint32_t>(functionIndex);
    // Reset instruction index
    m_currentInstructionIndex = 0;
    // Set new runtime stack base index for the called function, modified by
    // the function parameter size
    m_currentRuntimeStackBaseIndex =
        runtimeStackSize - newCurrentFunction.parameterSize;
    // Resize runtime stack with local stack size of called function
    m_runtimeStack.resize(static_cast<size_t>(newSize));
    break;
  }

  case Opcode::Calle:
  {
    // Call external, arg 0 is extern function index
    const int32_t externIndex = instruction.args[0];
    if (externIndex < 0 ||
        static_cast<uint64_t>(externIndex) >= m_externFunctions.size())
    {
      throwRuntimeError(m_logger, "Calle extern function index out of range.");
    }
    const ExternFunction &externFunction =
        m_externFunctions.at(static_cast<size_t>(externIndex));

    // Check if function exists
    // TODO Check if arg count ok
    auto entry = m_externFunctionsMap.find(externFunction.name);
    if (entry == m_externFunctionsMap.end())
    {
      // Function does not exist
      throwRuntimeError(m_logger, "The extern function '{}' does not exist.",
                        externFunction.name);
    }
    // Call if found, a failing call stops the script instead of continuing
    // with missing values on the stack.
    if (!entry->second->call(*this))
    {
      throwRuntimeError(m_logger, "The extern function '{}' reported failure.",
                        externFunction.name);
    }
    ++m_currentInstructionIndex;
    break;
  }
  case Opcode::Ret:
    // Return from script function
    if (m_callStack.empty())
    {
      // Empty stack indicates either error state or end of script
      return false;
    }
    // Resize runtime stack to remove local function variables.
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex);

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;

  case Opcode::Retv:
  {
    // Returns variable from script function
    if (m_callStack.empty())
    {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Retv without a caller.");
    }
    // Arg 0 is local variable index
    // The caller's stack ends at the runtime stack base index, so slot base
    // holds the first argument. It is the slot the caller reads the return
    // value from once the frame is dropped, so the return value goes there.
    uint32_t varIndex = 0;
    memcpy(&varIndex, &instruction.args[0], sizeof(varIndex));
    if (varIndex == 0 &&
        m_currentRuntimeStackBaseIndex >= m_runtimeStack.size())
    {
      // The frame is empty, so the return slot does not exist. Growing the
      // stack below would plant an Invalid value as the return value.
      throwRuntimeError(m_logger, "Retv variable index out of range.");
    }
    if (varIndex != 0)
    {
      if (!indexInRange(m_currentRuntimeStackBaseIndex,
                        instruction.args[0], m_runtimeStack.size()))
      {
        throwRuntimeError(m_logger, "Retv variable index out of range.");
      }
      m_runtimeStack[m_currentRuntimeStackBaseIndex] =
          m_runtimeStack.at(m_currentRuntimeStackBaseIndex + varIndex);
    }

    // Resize runtime stack to remove local function variables but the one
    // holding the return value. The resize can only grow here when the frame
    // is empty, so it stays within budget by the same argument as below.
    if (static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + 1 >
        m_maxStackSize)
    {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case Opcode::Reti:
  {
    // Returns variable from script function
    if (m_callStack.empty())
    {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Reti without a caller.");
    }
    // Arg 0 is integer constant
    // The caller's stack ends at the runtime stack base index, so slot base
    // is the slot the caller reads the return value from. A function without
    // parameters or locals leaves the frame empty, so the slot is grown
    // first rather than assumed to exist.
    if (static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + 1 >
        m_maxStackSize)
    {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);
    int32_t intValue = 0;
    memcpy(&intValue, &instruction.args[0], sizeof(intValue));
    m_runtimeStack[m_currentRuntimeStackBaseIndex] = intValue;

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case Opcode::Retf:
  {
    // Returns float from script function
    if (m_callStack.empty())
    {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Retf without a caller.");
    }
    // Arg 0 is float constant
    // The caller's stack ends at the runtime stack base index, so slot base
    // is the slot the caller reads the return value from. A function without
    // parameters or locals leaves the frame empty, so the slot is grown
    // first rather than assumed to exist.
    if (static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + 1 >
        m_maxStackSize)
    {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);
    float floatValue = 0.f;
    memcpy(&floatValue, &instruction.args[0], sizeof(floatValue));
    m_runtimeStack[m_currentRuntimeStackBaseIndex] = floatValue;

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case Opcode::Rets:
  {
    // Returns string from script function
    if (m_callStack.empty())
    {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Rets without a caller.");
    }
    // Arg 0 is index of string
    // The caller's stack ends at the runtime stack base index, so slot base
    // is the slot the caller reads the return value from. A function without
    // parameters or locals leaves the frame empty, so the slot is grown
    // first rather than assumed to exist. The string index is validated
    // before growing, so corrupt input cannot leave a grown frame behind.
    if (!indexInRange(0, instruction.args[0], m_strings.size()))
    {
      throwRuntimeError(m_logger, "Rets string index out of range.");
    }
    if (static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + 1 >
        m_maxStackSize)
    {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);
    uint32_t stringIndex = 0;
    memcpy(&stringIndex, &instruction.args[0], sizeof(stringIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex] =
        m_strings.at(stringIndex);

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case Opcode::Add:
  {
    Value x;
    // 2 Values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Add needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric())
    {
      throwRuntimeError(m_logger, "Add needs numeric operands.");
    }
    m_runtimeStack.back() += x;
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Sub:
  {
    Value x;
    // 2 Values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Sub needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric())
    {
      throwRuntimeError(m_logger, "Sub needs numeric operands.");
    }
    m_runtimeStack.back() -= x;
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Mul:
  {
    Value x;
    // 2 values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Mul needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric())
    {
      throwRuntimeError(m_logger, "Mul needs numeric operands.");
    }
    m_runtimeStack.back() *= x;
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Div:
  {
    Value x;
    // 2 values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Div needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric())
    {
      throwRuntimeError(m_logger, "Div needs numeric operands.");
    }
    // Value leaves the operand unchanged on a zero divisor instead of
    // dividing, so the check belongs to the caller.
    if (x.isZero())
    {
      throwRuntimeError(m_logger, "Division by zero.");
    }
    m_runtimeStack.back() /= x;
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Inc:
  {
    // Increments the top of the runtime stack in place.
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Inc needs a value on the runtime stack.");
    }
    Value &top = m_runtimeStack.back();
    if (!top.isNumeric())
    {
      throwRuntimeError(m_logger, "Inc needs a numeric value.");
    }
    top += Value(static_cast<int32_t>(1));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Dec:
  {
    // Decrements the top of the runtime stack in place.
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Dec needs a value on the runtime stack.");
    }
    Value &top = m_runtimeStack.back();
    if (!top.isNumeric())
    {
      throwRuntimeError(m_logger, "Dec needs a numeric value.");
    }
    top -= Value(static_cast<int32_t>(1));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::And:
  {
    Value x;
    Value y;
    if (!popValue(x) || !popValue(y))
    {
      throwRuntimeError(m_logger, "And needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric())
    {
      throwRuntimeError(m_logger, "And needs numeric values.");
    }
    // Zero is the only false value, any other value is true.
    pushValue(Value(static_cast<int32_t>(!x.isZero() && !y.isZero())));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Or:
  {
    Value x;
    Value y;
    if (!popValue(x) || !popValue(y))
    {
      throwRuntimeError(m_logger, "Or needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric())
    {
      throwRuntimeError(m_logger, "Or needs numeric values.");
    }
    pushValue(Value(static_cast<int32_t>(!x.isZero() || !y.isZero())));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Not:
  {
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Not needs a value on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric())
    {
      throwRuntimeError(m_logger, "Not needs a numeric value.");
    }
    const bool truth = !m_runtimeStack.back().isZero();
    m_runtimeStack.back() = Value(static_cast<int32_t>(!truth));
    ++m_currentInstructionIndex;
  }
  break;
  case Opcode::Xor:
  {
    Value x;
    Value y;
    if (!popValue(x) || !popValue(y))
    {
      throwRuntimeError(m_logger, "Xor needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric())
    {
      throwRuntimeError(m_logger, "Xor needs numeric values.");
    }
    // True only when exactly one side is true.
    const bool truth = (!x.isZero()) != (!y.isZero());
    pushValue(Value(static_cast<int32_t>(truth)));
    ++m_currentInstructionIndex;
  }
  break;

  case Opcode::Jmp:
  {
    // Arg 0 is target instruction index for jump. The end of the function
    // itself is a legal target: the fetch treats it as an implicit return,
    // which is how trailing labels resolve.
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize)
    {
      throwRuntimeError(m_logger, "Jmp target out of range.");
    }
    m_currentInstructionIndex = jumpIndex;
  }
  break;
  case Opcode::Je:
  {
    // Compare top 2 values from stack, x == y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Je needs two values on the runtime stack.");
    }
    if (x == y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = 0;
      memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
      if (jumpIndex > currentFunction.instructionSize)
      {
        throwRuntimeError(m_logger, "Je target out of range.");
      }
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  case Opcode::Jne:
  {
    // Compare top 2 values from stack, x != y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jne needs two values on the runtime stack.");
    }
    if (x != y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = 0;
      memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
      if (jumpIndex > currentFunction.instructionSize)
      {
        throwRuntimeError(m_logger, "Jne target out of range.");
      }
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  case Opcode::Jle:
  {
    // Compare top 2 values from stack, x <= y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jle needs two values on the runtime stack.");
    }
    if (x <= y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = 0;
      memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
      if (jumpIndex > currentFunction.instructionSize)
      {
        throwRuntimeError(m_logger, "Jle target out of range.");
      }
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  case Opcode::Jg:
  {
    // Compare top 2 values from stack, x > y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jg needs two values on the runtime stack.");
    }
    if (x > y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = 0;
      memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
      if (jumpIndex > currentFunction.instructionSize)
      {
        throwRuntimeError(m_logger, "Jg target out of range.");
      }
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  case Opcode::Jge:
  {
    // Compare top 2 values from stack, x >= y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jge needs two values on the runtime stack.");
    }
    if (x >= y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = 0;
      memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
      if (jumpIndex > currentFunction.instructionSize)
      {
        throwRuntimeError(m_logger, "Jge target out of range.");
      }
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;
  case Opcode::Jl:
  {
    // Compare top 2 values from stack, x < y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jl needs two values on the runtime stack.");
    }
    if (x < y)
    {
      // Arg 0 is target instruction index for jump
      uint32_t jumpIndex = 0;
      memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
      if (jumpIndex > currentFunction.instructionSize)
      {
        throwRuntimeError(m_logger, "Jl target out of range.");
      }
      m_currentInstructionIndex = jumpIndex;
    }
    else
    {
      ++m_currentInstructionIndex;
    }
  }
  break;

  default:
    // Unreachable through assembled code, reachable through corrupt bytecode
    throwRuntimeError(m_logger, "Unknown instruction id {}.",
                      static_cast<int>(instruction.id));
  }

  return true;
}

void VirtualMachine::pushValue(const Value &val)
{
  assert(val.getType() != Value::Type::Invalid);
  if (m_runtimeStack.size() >= m_maxStackSize)
  {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
  }
  m_runtimeStack.push_back(val);
}

bool VirtualMachine::popValue(Value &val)
{
  if (m_runtimeStack.empty())
  {
    return false;
  }
  val = std::move(m_runtimeStack.back());
  assert(val.getType() != Value::Type::Invalid);
  m_runtimeStack.pop_back();
  return true;
}
