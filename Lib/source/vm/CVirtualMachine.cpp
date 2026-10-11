#include <cassert>

#include "CVirtualMachine.h"

#include "kern/RuntimeError.h"

#include "common/Instructions.h"
#include "common/Deserialize.h"
#include "logging/CLogger.h"

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

}

CVirtualMachine::SFunctionFrame::SFunctionFrame() {}

CVirtualMachine::SFunctionFrame::SFunctionFrame(uint32_t funcIndex,
                                                 uint32_t instrIndex,
                                                 uint32_t stackBaseIndex)
{
  functionIndex = funcIndex;
  instructionIndex = instrIndex;
  runtimeStackBaseIndex = stackBaseIndex;
}

CVirtualMachine::CVirtualMachine(kern::ILogger *logger)
    : m_logger(logger != nullptr ? logger : nullLogger())
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
  if (!getFunctionIndexByName(functionName, m_currentFunctionIndex))
  {
    return false;
  }
  m_currentInstructionIndex = 0;
  m_runtimeStack.resize(m_functions.at(m_currentFunctionIndex).stackSize);

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
  if (m_runtimeStack.empty())
  {
    throwRuntimeError(m_logger,
                      "popParameter needs a value on the runtime stack.");
  }
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool CVirtualMachine::popParameter(float &value)
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

bool CVirtualMachine::popParameter(::std::string &value)
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

void CVirtualMachine::setReturnValue() { return; }

void CVirtualMachine::setReturnValue(int value) { pushValue(CValue(value)); }

void CVirtualMachine::setReturnValue(float value) { pushValue(CValue(value)); }

void CVirtualMachine::setReturnValue(const std::string &value)
{
  pushValue(CValue(value));
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
    // Read parameter size
    stream.read((char *)&function.parameterSize,
                sizeof(function.parameterSize));

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
    // Cache size for fast lookup
    function.instructionSize = size;
  }
  return stream.good();
}

bool CVirtualMachine::getFunctionIndexByName(const std::string &functionName,
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

bool CVirtualMachine::getFunctionByName(
    const std::string &functionName,
    const CVirtualMachine::SFunction *function_out) const
{
  for (const auto &function : m_functions)
  {
    if (function.name == functionName)
    {
      function_out = &function;
      return true;
    }
  }
  return false;
}

bool CVirtualMachine::execute()
{
  // Cache current function
  const SFunction &currentFunction = m_functions.at(m_currentFunctionIndex);

  // Default current instruction to implicit return
  SInstruction instruction;
  instruction.id = EInstruction::Ret;

  // Check for out of bounds instruction index
  if (currentFunction.instructionSize > m_currentInstructionIndex)
  {
    // Retrieve current instruction
    instruction = currentFunction.instructions.at(m_currentInstructionIndex);
  }

  switch (instruction.id)
  {
  case EInstruction::Nop:
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Break:
    // Breakpoint, for debugging. There is no debugger attached, so this
    // intentionally does nothing and execution continues.
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
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Movi:
    // Move integer to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source integer value
    m_runtimeStack[m_currentRuntimeStackBaseIndex +
                   *((uint32_t *)&instruction.args[0])] = instruction.args[1];
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Movf:
    // Move float to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source float value
    m_runtimeStack[m_currentRuntimeStackBaseIndex +
                   *((uint32_t *)&instruction.args[0])] =
        *((float *)&instruction.args[1]);
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Movs:
    // Move string to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source index of string
    m_runtimeStack[m_currentRuntimeStackBaseIndex +
                   *((uint32_t *)&instruction.args[0])] =
        m_strings.at(*((uint32_t *)&instruction.args[1]));
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Pushi:
    // Push signed 32 bit integer value to
    // Arg 0 is integer value
    pushValue(CValue(instruction.args[0]));
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Pushu:
    // Arg 0 is an unsigned integer value
    pushValue(CValue(*((uint32_t *)&instruction.args[0])));
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Pushs:
    // Arg 0 is string index
    pushValue(m_strings.at(*((uint32_t *)&instruction.args[0])));
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Pushf:
    // Arg 0 is float value
    pushValue(CValue(*((float *)&instruction.args[0])));
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Pushv:
    // Arg 0 is variable index
    pushValue(m_runtimeStack.at(m_currentRuntimeStackBaseIndex +
                                *((uint32_t *)&instruction.args[0])));
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Pop:
    // Remove top element from runtime stack
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Pop needs a value on the runtime stack.");
    }
    m_runtimeStack.pop_back();
    ++m_currentInstructionIndex;
    break;

  case EInstruction::Popv:
  {
    // Remove top of the stack and store it in variable
    // Arg 0 is variable index
    m_runtimeStack[m_currentRuntimeStackBaseIndex +
                   *((uint32_t *)&instruction.args[0])] = m_runtimeStack.back();
    m_runtimeStack.pop_back();
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::Call:
  {
    // Push current function index, next instruction index and base stack index
    // for
    // return call.
    m_callStack.push(SFunctionFrame(m_currentFunctionIndex,
                                    m_currentInstructionIndex + 1,
                                    m_currentRuntimeStackBaseIndex));
    // Arg 0 is function index of the called function
    m_currentFunctionIndex = *((uint32_t *)&instruction.args[0]);
    // Reset instruction index
    m_currentInstructionIndex = 0;
    // Set new runtime stack base index for the called function
    uint32_t runtimeStackSize = static_cast<uint32_t>(m_runtimeStack.size());
    m_currentRuntimeStackBaseIndex = runtimeStackSize;
    // Modify by function parameter size
    const SFunction &newCurrentFunction =
        m_functions.at(m_currentFunctionIndex);
    m_currentRuntimeStackBaseIndex -= newCurrentFunction.parameterSize;
    // Resize runtime stack with local stack size of called function
    m_runtimeStack.resize(runtimeStackSize + newCurrentFunction.stackSize -
                          newCurrentFunction.parameterSize);
    break;
  }

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
  case EInstruction::Ret:
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

  case EInstruction::Retv:
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
    uint32_t varIndex = *((uint32_t *)&instruction.args[0]);
    if (varIndex != 0)
    {
      m_runtimeStack[m_currentRuntimeStackBaseIndex] =
          m_runtimeStack.at(m_currentRuntimeStackBaseIndex + varIndex);
    }

    // Resize runtime stack to remove local function variables but the one
    // holding the return value.
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

  case EInstruction::Reti:
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
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);
    m_runtimeStack[m_currentRuntimeStackBaseIndex] =
        *((int32_t *)&instruction.args[0]);

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case EInstruction::Retf:
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
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);
    m_runtimeStack[m_currentRuntimeStackBaseIndex] =
        *((float *)&instruction.args[0]);

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case EInstruction::Rets:
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
    // first rather than assumed to exist.
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);
    m_runtimeStack[m_currentRuntimeStackBaseIndex] =
        m_strings.at(*((uint32_t *)&instruction.args[0]));

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case EInstruction::Add:
  {
    CValue x;
    // 2 Values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Add needs two values on the runtime stack.");
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
      throwRuntimeError(m_logger, "Sub needs two values on the runtime stack.");
    }
    m_runtimeStack.back() -= x;
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::Mul:
  {
    CValue x;
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

  case EInstruction::Div:
  {
    CValue x;
    // 2 values needed
    if (!popValue(x) || m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Div needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric())
    {
      throwRuntimeError(m_logger, "Div needs numeric operands.");
    }
    // CValue leaves the operand unchanged on a zero divisor instead of
    // dividing, so the check belongs to the caller.
    if (x.isZero())
    {
      throwRuntimeError(m_logger, "Division by zero.");
    }
    m_runtimeStack.back() /= x;
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::Inc:
  {
    // Increments the top of the runtime stack in place.
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Inc needs a value on the runtime stack.");
    }
    CValue &top = m_runtimeStack.back();
    if (!top.isNumeric())
    {
      throwRuntimeError(m_logger, "Inc needs a numeric value.");
    }
    top += CValue(static_cast<int32_t>(1));
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::Dec:
  {
    // Decrements the top of the runtime stack in place.
    if (m_runtimeStack.empty())
    {
      throwRuntimeError(m_logger, "Dec needs a value on the runtime stack.");
    }
    CValue &top = m_runtimeStack.back();
    if (!top.isNumeric())
    {
      throwRuntimeError(m_logger, "Dec needs a numeric value.");
    }
    top -= CValue(static_cast<int32_t>(1));
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::And:
  {
    CValue x;
    CValue y;
    if (!popValue(x) || !popValue(y))
    {
      throwRuntimeError(m_logger, "And needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric())
    {
      throwRuntimeError(m_logger, "And needs numeric values.");
    }
    // Zero is the only false value, any other value is true.
    pushValue(CValue(static_cast<int32_t>(!x.isZero() && !y.isZero())));
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::Or:
  {
    CValue x;
    CValue y;
    if (!popValue(x) || !popValue(y))
    {
      throwRuntimeError(m_logger, "Or needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric())
    {
      throwRuntimeError(m_logger, "Or needs numeric values.");
    }
    pushValue(CValue(static_cast<int32_t>(!x.isZero() || !y.isZero())));
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::Not:
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
    m_runtimeStack.back() = CValue(static_cast<int32_t>(!truth));
    ++m_currentInstructionIndex;
  }
  break;
  case EInstruction::Xor:
  {
    CValue x;
    CValue y;
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
    pushValue(CValue(static_cast<int32_t>(truth)));
    ++m_currentInstructionIndex;
  }
  break;

  case EInstruction::Jmp:
  {
    // Arg 0 is target instruction index for jump
    uint32_t jumpIndex = *((uint32_t *)&instruction.args[0]);
    m_currentInstructionIndex = jumpIndex;
  }
  break;
  case EInstruction::Je:
  {
    // Compare top 2 values from stack, x == y
    CValue y;
    CValue x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Je needs two values on the runtime stack.");
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
      throwRuntimeError(m_logger, "Jne needs two values on the runtime stack.");
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
      throwRuntimeError(m_logger, "Jle needs two values on the runtime stack.");
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
  case EInstruction::Jg:
  {
    // Compare top 2 values from stack, x > y
    CValue y;
    CValue x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jg needs two values on the runtime stack.");
    }
    if (x > y)
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
  case EInstruction::Jge:
  {
    // Compare top 2 values from stack, x >= y
    CValue y;
    CValue x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jge needs two values on the runtime stack.");
    }
    if (x >= y)
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
  case EInstruction::Jl:
  {
    // Compare top 2 values from stack, x < y
    CValue y;
    CValue x;
    if (!popValue(y) || !popValue(x))
    {
      throwRuntimeError(m_logger, "Jl needs two values on the runtime stack.");
    }
    if (x < y)
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

  default:
    // Unreachable through assembled code, reachable through corrupt bytecode
    throwRuntimeError(m_logger, "Unknown instruction id {}.",
                      static_cast<int>(instruction.id));
  }

  return true;
}

void CVirtualMachine::pushValue(const CValue &val)
{
  assert(val.getType() != CValue::EType::Invalid);
  m_runtimeStack.push_back(val);
}

bool CVirtualMachine::popValue(CValue &val)
{
  if (m_runtimeStack.empty())
  {
    return false;
  }
  val = std::move(m_runtimeStack.back());
  assert(val.getType() != CValue::EType::Invalid);
  m_runtimeStack.pop_back();
  return true;
}
