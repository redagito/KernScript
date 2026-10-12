#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>

#include "vm/VirtualMachine.h"

#include "kern/RuntimeError.h"

#include "common/Deserialize.h"
#include "common/Instructions.h"
#include "common/ScriptHeader.h"
#include "common/Version.h"
#include "logging/Logger.h"

namespace {

/**
 * \brief Logs a runtime error and throws it.
 *
 * Every error path in execute() goes through here, so diagnostics keep flowing
 * to the logger while the failure itself is a typed exception that tests and
 * hosts assert on instead of matching message text.
 */
template <typename... Args>
[[noreturn]] void throwRuntimeError(kern::ILogger *logger, fmt::format_string<Args...> format, Args &&...args) {
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
bool indexInRange(uint32_t base, int32_t rawIndex, size_t size) {
  const uint64_t index = static_cast<uint64_t>(base) + static_cast<uint64_t>(static_cast<uint32_t>(rawIndex));
  return index < static_cast<uint64_t>(size);
}

/**
 * \brief Checks an index read from bytecode against the current frame.
 *
 * Like indexInRange, but bound to the frame end (base + frameSize) instead of
 * the stack size, so a corrupt variable index cannot reach pushed temps above
 * the frame. The stack size still bounds too: arithmetic opcodes can eat into
 * the frame, leaving the end above the stack.
 */
bool frameIndexInRange(uint32_t base, int32_t rawIndex, uint32_t frameSize, size_t stackSize) {
  const uint64_t index = static_cast<uint64_t>(base) + static_cast<uint64_t>(static_cast<uint32_t>(rawIndex));
  const uint64_t frameEnd = static_cast<uint64_t>(base) + static_cast<uint64_t>(frameSize);
  return index < frameEnd && index < static_cast<uint64_t>(stackSize);
}

/**
 * \brief Refuses a host call made outside an extern dispatch.
 *
 * popParameter and setReturnValue are the extern function interface: they
 * only make sense while a script calle runs, like the other mutators that
 * check m_running.
 */
void requireDispatch(kern::ILogger *logger, bool running, const char *what) {
  if (!running) {
    throwRuntimeError(logger, "{} outside an extern call.", what);
  }
}

/**
 * \brief Checks a popParameter call before it touches the stack.
 *
 * Besides the dispatch and empty checks, popping must stop at the frame
 * floor: the frame holds base + frameSize slots, so a stack at or below that
 * end has no argument left to take.
 */
void checkPopAllowed(kern::ILogger *logger, bool running, const std::vector<Value> &stack, uint32_t base,
                     uint32_t frameSize) {
  requireDispatch(logger, running, "popParameter");
  if (stack.empty()) {
    throwRuntimeError(logger, "popParameter needs a value on the runtime stack.");
  }
  if (static_cast<uint64_t>(stack.size()) <= static_cast<uint64_t>(base) + static_cast<uint64_t>(frameSize)) {
    throwRuntimeError(logger, "popParameter would go below the frame.");
  }
}

/**
 * \brief Holds the callFunction reentrancy guard for the duration of a call.
 *
 * Resetting in the destructor covers both normal return and unwinding through
 * a script error, so one throw cannot wedge later calls.
 */
class CallGuard {
public:
  explicit CallGuard(bool &flag) : m_flag(flag) { m_flag = true; }
  ~CallGuard() { m_flag = false; }

  CallGuard(const CallGuard &) = delete;
  CallGuard &operator=(const CallGuard &) = delete;

private:
  bool &m_flag;
};

} // namespace

VirtualMachine::FunctionFrame::FunctionFrame() {}

VirtualMachine::FunctionFrame::FunctionFrame(uint32_t funcIndex, uint32_t instrIndex, uint32_t stackBaseIndex) {
  functionIndex = funcIndex;
  instructionIndex = instrIndex;
  runtimeStackBaseIndex = stackBaseIndex;
}

VirtualMachine::VirtualMachine(kern::ILogger *logger) : VirtualMachine(logger, kern::VMConfig{}) {}

VirtualMachine::VirtualMachine(kern::ILogger *logger, kern::VMConfig config)
    : m_logger(logger != nullptr ? logger : nullLogger()), m_maxStackSize(config.maxStackBytes / sizeof(Value)),
      m_maxCallDepth(config.maxCallDepth), m_maxStringBytes(config.maxStringBytes),
      m_maxTableEntries(config.maxTableEntries), m_maxExecutionSteps(config.maxExecutionSteps) {
  // Preallocate the whole budget, so steady state never reallocates. An
  // absurd budget fails here as a typed error instead of bad_alloc.
  try {
    m_runtimeStack.reserve(m_maxStackSize);
  } catch (const std::bad_alloc &) {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeds the limit.");
  } catch (const std::length_error &) {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeds the limit.");
  }
}

bool VirtualMachine::load(std::istream &byteCode) {
  // Tables back the running call: reloading them mid-execution would
  // reallocate the vectors execute() holds references into.
  if (m_running) {
    throwRuntimeError(m_logger, "load is not reentrant.");
  }
  // Atomicity: discard any previously loaded script first, and wipe the
  // tables again on any failure below, so a failed load leaves empty tables
  // behind instead of a mix of old and partial new data. A failure either
  // returns false or throws kern::RuntimeError; both leave no script loaded.
  clearScripts();
  try {
    if (!loadTables(byteCode)) {
      clearScripts();
      return false;
    }
  } catch (...) {
    clearScripts();
    throw;
  }
  return true;
}

bool VirtualMachine::loadTables(std::istream &byteCode) {
  // The header leads the bytecode. Anything else is not runnable here.
  ScriptHeader header;
  if (!deserialize(header, byteCode, m_maxStringBytes)) {
    return false;
  }
  const uint8_t expectedMagic[4] = {'K', 'E', 'R', 'N'};
  if (memcmp(header.identifier, expectedMagic, sizeof(expectedMagic)) != 0) {
    return false;
  }
  // Major versions must match exactly; newer minor versions stay loadable.
  if (header.majorVersion != majorVersion) {
    return false;
  }
  // Load string constants
  if (!deserializeStrings(byteCode)) {
    return false;
  }
  // Load extern functions
  if (!deserializeExternFunctions(byteCode)) {
    return false;
  }
  // Load script functions
  if (!deserializeFunctions(byteCode)) {
    return false;
  }
  return true;
}

void VirtualMachine::clearScripts() {
  // Clearing mid-execution would free the vectors execute() holds references
  // into, so refuse instead of corrupting the running call.
  if (m_running) {
    throwRuntimeError(m_logger, "clearScripts is not reentrant.");
  }
  m_currentFunctionIndex = 0;
  m_currentInstructionIndex = 0;
  m_currentRuntimeStackBaseIndex = 0;
  // Clear script data
  m_strings.clear();
  m_externFunctions.clear();
  m_functions.clear();
  // Drop any values and frames a previous call left behind.
  m_runtimeStack.clear();
  while (!m_callStack.empty()) {
    m_callStack.pop();
  }
}

bool VirtualMachine::callFunction(const std::string &functionName) {
  // Calls must not nest: an extern function calling back in here would reset
  // the stacks of the still-running call.
  if (m_running) {
    throwRuntimeError(m_logger, "callFunction is not reentrant.");
  }
  // Retrieve function index if exists
  if (!getFunctionIndexByName(functionName, m_currentFunctionIndex)) {
    return false;
  }
  const CallGuard guard(m_running);
  // Start from a clean slate: a previous call may have thrown out of nested
  // frames, leaving stale values, frames and a base index behind. Clearing
  // first also keeps locals from leaking values a previous call left in the
  // reused slots; the resize below then value-initializes the whole frame.
  m_runtimeStack.clear();
  while (!m_callStack.empty()) {
    m_callStack.pop();
  }
  m_currentRuntimeStackBaseIndex = 0;
  m_currentInstructionIndex = 0;
  const uint32_t stackSize = m_functions.at(m_currentFunctionIndex).stackSize;
  if (stackSize > m_maxStackSize) {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
  }
  // The budget check passes when the configured budget itself is absurd, so
  // a failing allocation must still surface as a typed error instead of
  // bad_alloc, matching the deserialize pattern.
  try {
    m_runtimeStack.resize(stackSize);
  } catch (const std::bad_alloc &) {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
  } catch (const std::length_error &) {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
  }

  // Execute function
  bool running = true;
  // Every execute() runs exactly one instruction, so counting loop rounds
  // counts executed instructions. A backward jump to itself would otherwise
  // spin the host forever; exceeding the budget stops the script instead.
  // The counter is local, so every callFunction starts from zero.
  uint64_t executedSteps = 0;
  const uint64_t stepLimit = m_maxExecutionSteps;
  while (running) {
    // Single step execution until finished
    running = execute();
    if (++executedSteps > stepLimit) {
      throwRuntimeError(m_logger, "Maximum execution steps exceeded.");
    }
  }
  return true;
}

void VirtualMachine::addFunction(const std::string &functionName, kern::IExternFunction *function) {
  // Mutating the map mid-execution could rehash it while Calle holds an
  // iterator across the extern call.
  if (m_running) {
    // Ownership stays with the caller on failure, so the caller deletes the
    // pointer instead of the VM deleting it and causing a double-free.
    throwRuntimeError(m_logger, "addFunction is not reentrant.");
  }
  if (functionName.empty()) {
    throwRuntimeError(m_logger, "Extern function name must not be empty.");
  }
  // A null would sit in the map until Calle dereferences it past the
  // missing-registration guard, so refuse it here. Null transfers no
  // ownership, so the caller has nothing to delete.
  if (function == nullptr) {
    throwRuntimeError(m_logger, "Extern function must not be null.");
  }
  m_externFunctionsMap[functionName].reset(function);
}

void VirtualMachine::removeFunction(const ::std::string &functionName) {
  // Erasing mid-execution would destroy the extern object whose call method
  // is still on the stack.
  if (m_running) {
    throwRuntimeError(m_logger, "removeFunction is not reentrant.");
  }
  m_externFunctionsMap.erase(functionName);
}

bool VirtualMachine::popParameter(int &value) {
  checkPopAllowed(m_logger, m_running, m_runtimeStack, m_currentRuntimeStackBaseIndex,
                  m_functions.at(m_currentFunctionIndex).stackSize);
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool VirtualMachine::popParameter(float &value) {
  checkPopAllowed(m_logger, m_running, m_runtimeStack, m_currentRuntimeStackBaseIndex,
                  m_functions.at(m_currentFunctionIndex).stackSize);
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool VirtualMachine::popParameter(::std::string &value) {
  checkPopAllowed(m_logger, m_running, m_runtimeStack, m_currentRuntimeStackBaseIndex,
                  m_functions.at(m_currentFunctionIndex).stackSize);
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool VirtualMachine::popParameter(double &value) {
  checkPopAllowed(m_logger, m_running, m_runtimeStack, m_currentRuntimeStackBaseIndex,
                  m_functions.at(m_currentFunctionIndex).stackSize);
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

bool VirtualMachine::popParameter(unsigned int &value) {
  checkPopAllowed(m_logger, m_running, m_runtimeStack, m_currentRuntimeStackBaseIndex,
                  m_functions.at(m_currentFunctionIndex).stackSize);
  uint32_t raw = 0;
  bool ret = m_runtimeStack.back().convert(raw);
  value = static_cast<unsigned int>(raw);
  m_runtimeStack.pop_back();
  return ret;
}

bool VirtualMachine::popParameter(bool &value) {
  checkPopAllowed(m_logger, m_running, m_runtimeStack, m_currentRuntimeStackBaseIndex,
                  m_functions.at(m_currentFunctionIndex).stackSize);
  bool ret = m_runtimeStack.back().convert(value);
  m_runtimeStack.pop_back();
  return ret;
}

void VirtualMachine::setReturnValue() {
  requireDispatch(m_logger, m_running, "setReturnValue");
  return;
}

void VirtualMachine::setReturnValue(int value) {
  requireDispatch(m_logger, m_running, "setReturnValue");
  pushValue(Value(value));
}

void VirtualMachine::setReturnValue(float value) {
  requireDispatch(m_logger, m_running, "setReturnValue");
  pushValue(Value(value));
}

void VirtualMachine::setReturnValue(const std::string &value) {
  requireDispatch(m_logger, m_running, "setReturnValue");
  pushValue(Value(value));
}

void VirtualMachine::setReturnValue(double value) {
  requireDispatch(m_logger, m_running, "setReturnValue");
  // The runtime model stores floats, so doubles narrow here.
  pushValue(Value(static_cast<float>(value)));
}

void VirtualMachine::setReturnValue(unsigned int value) {
  requireDispatch(m_logger, m_running, "setReturnValue");
  pushValue(Value(static_cast<uint32_t>(value)));
}

void VirtualMachine::setReturnValue(bool value) {
  requireDispatch(m_logger, m_running, "setReturnValue");
  // Stored as 1 or 0, matching what the logical operators produce.
  pushValue(Value(static_cast<int32_t>(value)));
}

bool VirtualMachine::deserializeStrings(std::istream &stream) {
  // Get string table size and reserve space
  uint32_t size = 0;
  if (!deserialize(stream, size)) {
    return false;
  }
  // A corrupt count must not reach resize(), where it would throw bad_alloc
  // instead of a typed error.
  if (size > m_maxTableEntries) {
    throwRuntimeError(m_logger, "String table size {} exceeds the limit.", size);
  }
  // The limit check passes when the configured limit itself is absurd, so a
  // failing allocation must still surface as a typed error.
  try {
    m_strings.resize(size);
  } catch (const std::bad_alloc &) {
    throwRuntimeError(m_logger, "String table size {} exceeds the limit.", size);
  } catch (const std::length_error &) {
    throwRuntimeError(m_logger, "String table size {} exceeds the limit.", size);
  }

  // Read strings into string table
  for (auto &str : m_strings) {
    if (!deserialize(stream, str, m_maxStringBytes)) {
      return false;
    }
  }
  return true;
}

bool VirtualMachine::deserializeExternFunctions(std::istream &stream) {
  // Read size. A truncated count must fail before the value reaches the
  // limit check and resize, like deserializeStrings via checked deserialize.
  uint32_t size = 0;
  if (!deserialize(stream, size)) {
    return false;
  }
  if (size > m_maxTableEntries) {
    throwRuntimeError(m_logger, "Extern function count {} exceeds the limit.", size);
  }
  try {
    m_externFunctions.resize(size);
  } catch (const std::bad_alloc &) {
    throwRuntimeError(m_logger, "Extern function count {} exceeds the limit.", size);
  } catch (const std::length_error &) {
    throwRuntimeError(m_logger, "Extern function count {} exceeds the limit.", size);
  }
  for (auto &externFunction : m_externFunctions) {
    if (!deserialize(stream, externFunction, m_maxStringBytes)) {
      return false;
    }
  }
  return stream.good();
}

bool VirtualMachine::deserializeFunctions(std::istream &stream) {
  // Read size. A truncated count must fail before the value reaches the
  // limit check and resize.
  uint32_t size = 0;
  if (!deserialize(stream, size)) {
    return false;
  }
  if (size > m_maxTableEntries) {
    throwRuntimeError(m_logger, "Function count {} exceeds the limit.", size);
  }
  // Reserve space
  try {
    m_functions.resize(size);
  } catch (const std::bad_alloc &) {
    throwRuntimeError(m_logger, "Function count {} exceeds the limit.", size);
  } catch (const std::length_error &) {
    throwRuntimeError(m_logger, "Function count {} exceeds the limit.", size);
  }

  // Read function data
  for (auto &function : m_functions) {
    // Read function name. A truncated length must fail before the value
    // reaches the limit check and reserve.
    uint32_t length = 0;
    if (!deserialize(stream, length)) {
      return false;
    }
    if (length > m_maxStringBytes) {
      throwRuntimeError(m_logger, "Function name length {} exceeds the limit.", length);
    }
    // The limit check passes when the configured limit itself is absurd, so
    // a failing allocation must still surface as a typed error instead of
    // bad_alloc, matching the table-resize pattern. The name builds in a
    // local first, like generic strings, so a truncated stream returns false
    // without touching the target.
    std::string temp;
    try {
      temp.reserve(length);
    } catch (const std::bad_alloc &) {
      throwRuntimeError(m_logger, "Function name length {} exceeds the limit.", length);
    } catch (const std::length_error &) {
      throwRuntimeError(m_logger, "Function name length {} exceeds the limit.", length);
    }
    for (uint32_t i = 0; i < length; ++i) {
      char c = 0;
      stream.read(&c, 1);
      // A truncated name must fail cleanly instead of pushing an
      // indeterminate character and continuing.
      if (!stream.good()) {
        return false;
      }
      temp.push_back(c);
    }
    function.name = std::move(temp);

    // Read stack size. Each count must prove the stream still holds it
    // before the value is used; truncation returns false instead of
    // carrying a partial value forward.
    if (!deserialize(stream, function.stackSize)) {
      return false;
    }
    // Read parameter size
    if (!deserialize(stream, function.parameterSize)) {
      return false;
    }

    // Read instructions
    // Instructions size
    if (!deserialize(stream, size)) {
      return false;
    }
    if (size > m_maxTableEntries) {
      throwRuntimeError(m_logger, "Instruction count {} exceeds the limit.", size);
    }
    try {
      function.instructions.resize(size);
    } catch (const std::bad_alloc &) {
      throwRuntimeError(m_logger, "Instruction count {} exceeds the limit.", size);
    } catch (const std::length_error &) {
      throwRuntimeError(m_logger, "Instruction count {} exceeds the limit.", size);
    }
    // Instructions
    for (auto &instruction : function.instructions) {
      if (!deserialize(instruction, stream)) {
        return false;
      }
    }
    // Cache size for fast lookup
    function.instructionSize = size;
  }
  return stream.good();
}

bool VirtualMachine::getFunctionIndexByName(const std::string &functionName, uint32_t &index) {
  // Preserve semantics: the out-param is only assigned on success, so a
  // failed lookup leaves the caller's index (and a clean state) alone instead
  // of clobbering it with one-past-the-end.
  uint32_t found = 0;
  for (const auto &function : m_functions) {
    if (function.name == functionName) {
      index = found;
      return true;
    }
    ++found;
  }
  return false;
}

bool VirtualMachine::execute() {
  // Cache current function
  const Function &currentFunction = m_functions.at(m_currentFunctionIndex);

  // An instruction index past the last instruction implicitly returns instead
  // of throwing. This is deliberate: a trailing label resolves to exactly
  // this position, so a function ending in a label would otherwise need a
  // redundant ret, and an empty function would have no valid entry state.
  // Default current instruction to implicit return
  Instruction instruction;
  instruction.id = Opcode::Ret;

  // Check for out of bounds instruction index
  if (currentFunction.instructionSize > m_currentInstructionIndex) {
    // Retrieve current instruction
    instruction = currentFunction.instructions.at(m_currentInstructionIndex);
  }

  switch (instruction.id) {
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

  case Opcode::Movv: {
    // Move variable to variable
    // Arg 0 is variable index of destination
    // Arg 1 is variable index of source
    if (!frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0], currentFunction.stackSize,
                           m_runtimeStack.size()) ||
        !frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[1], currentFunction.stackSize,
                           m_runtimeStack.size())) {
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

  case Opcode::Movi: {
    // Move integer to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source integer value
    if (!frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0], currentFunction.stackSize,
                           m_runtimeStack.size())) {
      throwRuntimeError(m_logger, "Movi variable index out of range.");
    }
    uint32_t destIndex = 0;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] = instruction.args[1];
    ++m_currentInstructionIndex;
    break;
  }

  case Opcode::Movf: {
    // Move float to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source float value
    if (!frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0], currentFunction.stackSize,
                           m_runtimeStack.size())) {
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

  case Opcode::Movs: {
    // Move string to variable
    // Arg 0 is variable index of destination
    // Arg 1 is source index of string
    if (!frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0], currentFunction.stackSize,
                           m_runtimeStack.size())) {
      throwRuntimeError(m_logger, "Movs variable index out of range.");
    }
    if (!indexInRange(0, instruction.args[1], m_strings.size())) {
      throwRuntimeError(m_logger, "Movs string index out of range.");
    }
    uint32_t destIndex = 0;
    uint32_t stringIndex = 0;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    memcpy(&stringIndex, &instruction.args[1], sizeof(stringIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] = m_strings.at(stringIndex);
    ++m_currentInstructionIndex;
    break;
  }

  case Opcode::Pushi:
    // Push signed 32 bit integer value to
    // Arg 0 is integer value
    pushValue(Value(instruction.args[0]));
    ++m_currentInstructionIndex;
    break;

  case Opcode::Pushu: {
    // Arg 0 is an unsigned integer value
    uint32_t value = 0;
    memcpy(&value, &instruction.args[0], sizeof(value));
    pushValue(Value(value));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Pushs: {
    // Arg 0 is string index
    uint32_t stringIndex = 0;
    memcpy(&stringIndex, &instruction.args[0], sizeof(stringIndex));
    if (!indexInRange(0, instruction.args[0], m_strings.size())) {
      throwRuntimeError(m_logger, "Pushs string index out of range.");
    }
    pushValue(m_strings.at(stringIndex));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Pushf: {
    // Arg 0 is float value
    float value = 0.f;
    memcpy(&value, &instruction.args[0], sizeof(value));
    pushValue(Value(value));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Pushv: {
    // Arg 0 is variable index
    uint32_t varIndex = 0;
    memcpy(&varIndex, &instruction.args[0], sizeof(varIndex));
    if (!frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0], currentFunction.stackSize,
                           m_runtimeStack.size())) {
      throwRuntimeError(m_logger, "Pushv variable index out of range.");
    }
    pushValue(m_runtimeStack.at(m_currentRuntimeStackBaseIndex + varIndex));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Pop:
    // Remove top element from runtime stack
    if (m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Pop needs a value on the runtime stack.");
    }
    if (static_cast<uint64_t>(m_runtimeStack.size()) <=
        static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + static_cast<uint64_t>(currentFunction.stackSize)) {
      throwRuntimeError(m_logger, "Pop would go below the frame.");
    }
    m_runtimeStack.pop_back();
    ++m_currentInstructionIndex;
    break;

  case Opcode::Popv: {
    // Remove top of the stack and store it in variable
    // Arg 0 is variable index
    if (m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Popv needs a value on the runtime stack.");
    }
    if (static_cast<uint64_t>(m_runtimeStack.size()) <=
        static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + static_cast<uint64_t>(currentFunction.stackSize)) {
      throwRuntimeError(m_logger, "Popv would go below the frame.");
    }
    if (!frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0], currentFunction.stackSize,
                           m_runtimeStack.size())) {
      throwRuntimeError(m_logger, "Popv variable index out of range.");
    }
    // Copying would spread Invalid through assignment, so refuse it here
    // like pushValue and popValue do instead of erroring late.
    if (m_runtimeStack.back().getType() == Value::Type::Invalid) {
      throwRuntimeError(m_logger, "Popv of an uninitialized value.");
    }
    uint32_t destIndex = 0;
    memcpy(&destIndex, &instruction.args[0], sizeof(destIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex + destIndex] = m_runtimeStack.back();
    m_runtimeStack.pop_back();
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Call: {
    // Arg 0 is function index of the called function
    const int32_t functionIndex = instruction.args[0];
    if (functionIndex < 0 || static_cast<uint64_t>(functionIndex) >= m_functions.size()) {
      throwRuntimeError(m_logger, "Call function index out of range.");
    }
    const Function &newCurrentFunction = m_functions.at(static_cast<size_t>(functionIndex));
    const uint32_t runtimeStackSize = static_cast<uint32_t>(m_runtimeStack.size());
    if (newCurrentFunction.parameterSize > runtimeStackSize) {
      throwRuntimeError(m_logger, "Call needs {} parameters on the runtime stack.", newCurrentFunction.parameterSize);
    }
    if (m_callStack.size() >= m_maxCallDepth) {
      throwRuntimeError(m_logger, "Maximum call depth exceeded.");
    }
    // All arithmetic below runs in 64 bit: 32 bit stack sizes from corrupt
    // input wrap around and would otherwise resize the stack small.
    const uint64_t newSize = static_cast<uint64_t>(runtimeStackSize) +
                             static_cast<uint64_t>(newCurrentFunction.stackSize) -
                             static_cast<uint64_t>(newCurrentFunction.parameterSize);
    if (newSize > m_maxStackSize) {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    // Push current function index, next instruction index and base stack index
    // for
    // return call.
    m_callStack.push(
        FunctionFrame(m_currentFunctionIndex, m_currentInstructionIndex + 1, m_currentRuntimeStackBaseIndex));
    m_currentFunctionIndex = static_cast<uint32_t>(functionIndex);
    // Reset instruction index
    m_currentInstructionIndex = 0;
    // Set new runtime stack base index for the called function, modified by
    // the function parameter size
    m_currentRuntimeStackBaseIndex = runtimeStackSize - newCurrentFunction.parameterSize;
    // Resize runtime stack with local stack size of called function. The
    // budget check passes when the configured budget itself is absurd, so a
    // failing allocation must still surface as a typed error instead of
    // bad_alloc, matching the deserialize pattern.
    try {
      m_runtimeStack.resize(static_cast<size_t>(newSize));
    } catch (const std::bad_alloc &) {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    } catch (const std::length_error &) {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    break;
  }

  case Opcode::Calle: {
    // Call external, arg 0 is extern function index
    const int32_t externIndex = instruction.args[0];
    if (externIndex < 0 || static_cast<uint64_t>(externIndex) >= m_externFunctions.size()) {
      throwRuntimeError(m_logger, "Calle extern function index out of range.");
    }
    const ExternFunction &externFunction = m_externFunctions.at(static_cast<size_t>(externIndex));

    // Arguments are the values pushed above the current frame, whose slots
    // run from the base index for stackSize entries. Anything else on the
    // stack belongs to locals or parameters, not to this call.
    //
    // Pop discipline: return values a previous extern call left on the stack
    // count toward the next call, so scripts must consume or pop values they
    // do not need before pushing the next call's arguments. Counting only
    // script pushes since the last calle cannot distinguish such a stale
    // return from a legitimate return flowing directly into the next call
    // (calle get_value followed by calle println), which the stack-contents
    // count accepts; keeping the strict count is what makes both the
    // too-few/too-many refusals and the return-chaining pattern work.
    const uint64_t frameEnd =
        static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + static_cast<uint64_t>(currentFunction.stackSize);
    const uint64_t stackSizeNow = static_cast<uint64_t>(m_runtimeStack.size());
    if (stackSizeNow < frameEnd) {
      throwRuntimeError(m_logger, "The runtime stack is below the frame.");
    }
    const uint64_t pushed = stackSizeNow - frameEnd;
    if (pushed != externFunction.argCount) {
      throwRuntimeError(m_logger,
                        "The extern function '{}' expects {} arguments, but "
                        "{} were pushed.",
                        externFunction.name, externFunction.argCount, pushed);
    }

    // Check if function exists
    auto entry = m_externFunctionsMap.find(externFunction.name);
    if (entry == m_externFunctionsMap.end()) {
      // Function does not exist
      throwRuntimeError(m_logger, "The extern function '{}' does not exist.", externFunction.name);
    }
    // Call if found, a failing call stops the script instead of continuing
    // with missing values on the stack.
    if (!entry->second->call(*this)) {
      throwRuntimeError(m_logger, "The extern function '{}' reported failure.", externFunction.name);
    }
    ++m_currentInstructionIndex;
    break;
  }
  case Opcode::Ret:
    // Return from script function
    if (m_callStack.empty()) {
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

  case Opcode::Retv: {
    // Returns variable from script function
    if (m_callStack.empty()) {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Retv without a caller.");
    }
    // Arg 0 is local variable index
    // The caller's stack ends at the runtime stack base index, so slot base
    // holds the first argument. It is the slot the caller reads the return
    // value from once the frame is dropped, so the return value goes there.
    uint32_t varIndex = 0;
    memcpy(&varIndex, &instruction.args[0], sizeof(varIndex));
    if (varIndex == 0 && m_currentRuntimeStackBaseIndex >= m_runtimeStack.size()) {
      // The frame is empty, so the return slot does not exist. Growing the
      // stack below would plant an Invalid value as the return value.
      throwRuntimeError(m_logger, "Retv variable index out of range.");
    }
    if (varIndex != 0) {
      if (!frameIndexInRange(m_currentRuntimeStackBaseIndex, instruction.args[0], currentFunction.stackSize,
                             m_runtimeStack.size())) {
        throwRuntimeError(m_logger, "Retv variable index out of range.");
      }
      m_runtimeStack[m_currentRuntimeStackBaseIndex] = m_runtimeStack.at(m_currentRuntimeStackBaseIndex + varIndex);
    }

    // Resize runtime stack to remove local function variables but the one
    // holding the return value. No budget check: the empty frame returns
    // above, so base + 1 is at most the current size and this only shrinks.
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

  case Opcode::Reti: {
    // Returns variable from script function
    if (m_callStack.empty()) {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Reti without a caller.");
    }
    // Arg 0 is integer constant
    // The caller's stack ends at the runtime stack base index, so slot base
    // is the slot the caller reads the return value from. A function without
    // parameters or locals leaves the frame empty, so the slot is grown
    // first rather than assumed to exist.
    if (static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + 1 > m_maxStackSize) {
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

  case Opcode::Retf: {
    // Returns float from script function
    if (m_callStack.empty()) {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Retf without a caller.");
    }
    // Arg 0 is float constant
    // The caller's stack ends at the runtime stack base index, so slot base
    // is the slot the caller reads the return value from. A function without
    // parameters or locals leaves the frame empty, so the slot is grown
    // first rather than assumed to exist.
    if (static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + 1 > m_maxStackSize) {
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

  case Opcode::Rets: {
    // Returns string from script function
    if (m_callStack.empty()) {
      // No caller exists to receive the value
      throwRuntimeError(m_logger, "Rets without a caller.");
    }
    // Arg 0 is index of string
    // The caller's stack ends at the runtime stack base index, so slot base
    // is the slot the caller reads the return value from. A function without
    // parameters or locals leaves the frame empty, so the slot is grown
    // first rather than assumed to exist. The string index is validated
    // before growing, so corrupt input cannot leave a grown frame behind.
    if (!indexInRange(0, instruction.args[0], m_strings.size())) {
      throwRuntimeError(m_logger, "Rets string index out of range.");
    }
    if (static_cast<uint64_t>(m_currentRuntimeStackBaseIndex) + 1 > m_maxStackSize) {
      throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
    }
    m_runtimeStack.resize(m_currentRuntimeStackBaseIndex + 1);
    uint32_t stringIndex = 0;
    memcpy(&stringIndex, &instruction.args[0], sizeof(stringIndex));
    m_runtimeStack[m_currentRuntimeStackBaseIndex] = m_strings.at(stringIndex);

    // Retrieve function index of previous function
    m_currentFunctionIndex = m_callStack.top().functionIndex;
    // Restore active function index
    m_currentInstructionIndex = m_callStack.top().instructionIndex;
    // Restore runtime stack base index for the active function
    m_currentRuntimeStackBaseIndex = m_callStack.top().runtimeStackBaseIndex;
    m_callStack.pop();
    break;
  }

  case Opcode::Add: {
    Value x;
    // 2 Values needed
    if (!popValue(x) || m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Add needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric()) {
      throwRuntimeError(m_logger, "Add needs numeric operands.");
    }
    m_runtimeStack.back() += x;
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Sub: {
    Value x;
    // 2 Values needed
    if (!popValue(x) || m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Sub needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric()) {
      throwRuntimeError(m_logger, "Sub needs numeric operands.");
    }
    m_runtimeStack.back() -= x;
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Mul: {
    Value x;
    // 2 values needed
    if (!popValue(x) || m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Mul needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric()) {
      throwRuntimeError(m_logger, "Mul needs numeric operands.");
    }
    m_runtimeStack.back() *= x;
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Div: {
    Value x;
    // 2 values needed
    if (!popValue(x) || m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Div needs two values on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric() || !x.isNumeric()) {
      throwRuntimeError(m_logger, "Div needs numeric operands.");
    }
    // Value leaves the operand unchanged on a zero divisor instead of
    // dividing, so the check belongs to the caller.
    if (x.isZero()) {
      throwRuntimeError(m_logger, "Division by zero.");
    }
    m_runtimeStack.back() /= x;
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Inc: {
    // Increments the top of the runtime stack in place.
    if (m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Inc needs a value on the runtime stack.");
    }
    Value &top = m_runtimeStack.back();
    if (!top.isNumeric()) {
      throwRuntimeError(m_logger, "Inc needs a numeric value.");
    }
    top += Value(static_cast<int32_t>(1));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Dec: {
    // Decrements the top of the runtime stack in place.
    if (m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Dec needs a value on the runtime stack.");
    }
    Value &top = m_runtimeStack.back();
    if (!top.isNumeric()) {
      throwRuntimeError(m_logger, "Dec needs a numeric value.");
    }
    top -= Value(static_cast<int32_t>(1));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::And: {
    Value x;
    Value y;
    if (!popValue(x) || !popValue(y)) {
      throwRuntimeError(m_logger, "And needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric()) {
      throwRuntimeError(m_logger, "And needs numeric values.");
    }
    // Zero is the only false value, any other value is true.
    pushValue(Value(static_cast<int32_t>(!x.isZero() && !y.isZero())));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Or: {
    Value x;
    Value y;
    if (!popValue(x) || !popValue(y)) {
      throwRuntimeError(m_logger, "Or needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric()) {
      throwRuntimeError(m_logger, "Or needs numeric values.");
    }
    pushValue(Value(static_cast<int32_t>(!x.isZero() || !y.isZero())));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Not: {
    if (m_runtimeStack.empty()) {
      throwRuntimeError(m_logger, "Not needs a value on the runtime stack.");
    }
    if (!m_runtimeStack.back().isNumeric()) {
      throwRuntimeError(m_logger, "Not needs a numeric value.");
    }
    const bool truth = !m_runtimeStack.back().isZero();
    m_runtimeStack.back() = Value(static_cast<int32_t>(!truth));
    ++m_currentInstructionIndex;
  } break;
  case Opcode::Xor: {
    Value x;
    Value y;
    if (!popValue(x) || !popValue(y)) {
      throwRuntimeError(m_logger, "Xor needs two values on the runtime stack.");
    }
    if (!x.isNumeric() || !y.isNumeric()) {
      throwRuntimeError(m_logger, "Xor needs numeric values.");
    }
    // True only when exactly one side is true.
    const bool truth = (!x.isZero()) != (!y.isZero());
    pushValue(Value(static_cast<int32_t>(truth)));
    ++m_currentInstructionIndex;
  } break;

  case Opcode::Jmp: {
    // Arg 0 is target instruction index for jump. The end of the function
    // itself is a legal target: the fetch treats it as an implicit return,
    // which is how trailing labels resolve.
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize) {
      throwRuntimeError(m_logger, "Jmp target out of range.");
    }
    m_currentInstructionIndex = jumpIndex;
  } break;
  case Opcode::Je: {
    // The target is validated before the operands pop, so a refused jump
    // leaves the stack intact.
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize) {
      throwRuntimeError(m_logger, "Je target out of range.");
    }
    // Compare top 2 values from stack, x == y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x)) {
      throwRuntimeError(m_logger, "Je needs two values on the runtime stack.");
    }
    if (x == y) {
      m_currentInstructionIndex = jumpIndex;
    } else {
      ++m_currentInstructionIndex;
    }
  } break;
  case Opcode::Jne: {
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize) {
      throwRuntimeError(m_logger, "Jne target out of range.");
    }
    // Compare top 2 values from stack, x != y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x)) {
      throwRuntimeError(m_logger, "Jne needs two values on the runtime stack.");
    }
    if (x != y) {
      m_currentInstructionIndex = jumpIndex;
    } else {
      ++m_currentInstructionIndex;
    }
  } break;
  case Opcode::Jle: {
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize) {
      throwRuntimeError(m_logger, "Jle target out of range.");
    }
    // Compare top 2 values from stack, x <= y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x)) {
      throwRuntimeError(m_logger, "Jle needs two values on the runtime stack.");
    }
    if (x <= y) {
      m_currentInstructionIndex = jumpIndex;
    } else {
      ++m_currentInstructionIndex;
    }
  } break;
  case Opcode::Jg: {
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize) {
      throwRuntimeError(m_logger, "Jg target out of range.");
    }
    // Compare top 2 values from stack, x > y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x)) {
      throwRuntimeError(m_logger, "Jg needs two values on the runtime stack.");
    }
    if (x > y) {
      m_currentInstructionIndex = jumpIndex;
    } else {
      ++m_currentInstructionIndex;
    }
  } break;
  case Opcode::Jge: {
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize) {
      throwRuntimeError(m_logger, "Jge target out of range.");
    }
    // Compare top 2 values from stack, x >= y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x)) {
      throwRuntimeError(m_logger, "Jge needs two values on the runtime stack.");
    }
    if (x >= y) {
      m_currentInstructionIndex = jumpIndex;
    } else {
      ++m_currentInstructionIndex;
    }
  } break;
  case Opcode::Jl: {
    uint32_t jumpIndex = 0;
    memcpy(&jumpIndex, &instruction.args[0], sizeof(jumpIndex));
    if (jumpIndex > currentFunction.instructionSize) {
      throwRuntimeError(m_logger, "Jl target out of range.");
    }
    // Compare top 2 values from stack, x < y
    Value y;
    Value x;
    if (!popValue(y) || !popValue(x)) {
      throwRuntimeError(m_logger, "Jl needs two values on the runtime stack.");
    }
    if (x < y) {
      m_currentInstructionIndex = jumpIndex;
    } else {
      ++m_currentInstructionIndex;
    }
  } break;

  default:
    // Unreachable through assembled code, reachable through corrupt bytecode
    throwRuntimeError(m_logger, "Unknown instruction id {}.", static_cast<int>(instruction.id));
  }

  return true;
}

void VirtualMachine::pushValue(const Value &val) {
  if (val.getType() == Value::Type::Invalid) {
    throwRuntimeError(m_logger, "Push of an uninitialized value.");
  }
  if (m_runtimeStack.size() >= m_maxStackSize) {
    throwRuntimeError(m_logger, "Maximum runtime stack size exceeded.");
  }
  m_runtimeStack.push_back(val);
}

bool VirtualMachine::popValue(Value &val) {
  if (m_runtimeStack.empty()) {
    return false;
  }
  if (m_runtimeStack.back().getType() == Value::Type::Invalid) {
    throwRuntimeError(m_logger, "Pop of an uninitialized value.");
  }
  val = std::move(m_runtimeStack.back());
  m_runtimeStack.pop_back();
  return true;
}
