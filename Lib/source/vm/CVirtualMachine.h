#pragma once

#include <stack>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

#include "kern/IVirtualMachine.h"
#include "kern/IExternFunction.h"

#include "CValue.h"
#include "common/SInstruction.h"
#include "common/SExternFunction.h"

class CVirtualMachine : public kern::IVirtualMachine
{
public:
  CVirtualMachine();

  bool load(std::istream &byteCode);

  void clearScripts();

  bool callFunction(const std::string &functionName);

  void addFunction(const std::string &functionName,
                   kern::IExternFunction *function);
  void removeFunction(const std::string &functionName);

  bool popParameter(int &value);
  bool popParameter(float &value);
  bool popParameter(std::string &value);

  void setReturnValue();
  void setReturnValue(int value);
  void setReturnValue(float value);
  void setReturnValue(const std::string &value);

protected:
  /**
  * \brief Byte code deserialization.
  */
  bool deserializeStrings(std::istream &stream);
  bool deserializeExternFunctions(std::istream &stream);
  bool deserializeFunctions(std::istream &stream);

  bool getFunctionIndexByName(const std::string &functionName, uint32_t &index);

  bool execute();

  /**
  * \brief Pushes value on top of runtime stack.
  */
  void pushValue(const CValue &value);

  /**
  * \brief Pops value from runtime stack into supplied variable.
  */
  bool popValue(CValue &val);

  void printRuntimeStack() const;

private:
  /**
  * \brief Assembled function.
  */
  struct SFunction
  {
    std::string name; /**< Function name. */
    uint32_t stackSize =
        0; /**< Stack size needed for local variables and passed parameters. */
    uint32_t parameterSize = 0; /**< Stack size needed for parameters. */
    std::vector<SInstruction> instructions; /**< Assembled instructions. */
    uint32_t instructionSize = 0;           /**< Cached instruction size. */
  };

  /**
   * Gets function by name.
   */
  bool getFunctionByName(const std::string &functionName,
                         const SFunction *function) const;

  /**
  * \brief Represents function and instruction index to return to, after
  * instruction ret is executed.
  */
  struct SFunctionFrame
  {
    SFunctionFrame();
    SFunctionFrame(uint32_t funcIndex, uint32_t instrIndex,
                   uint32_t stackBaseIndex);
    uint32_t functionIndex = 0;
    uint32_t instructionIndex = 0;
    uint32_t runtimeStackBaseIndex = 0;
  };

  std::vector<std::string> m_strings; /**< String constants. */
  std::vector<SFunction> m_functions; /**< Assembled script functions. */
  std::vector<SExternFunction> m_externFunctions;

  std::unordered_map<std::string, std::unique_ptr<kern::IExternFunction>>
      m_externFunctionsMap; /**< Extern functions. */

  std::vector<CValue> m_runtimeStack; /**< Active runtime stack. */
  std::stack<SFunctionFrame>
      m_callStack; /**< Function call stack for instruction ret. */

  uint32_t m_currentRuntimeStackBaseIndex =
      0; /**< Runtime stack base index of current function. */
  uint32_t m_currentFunctionIndex = 0;    /**< Index of active function. */
  uint32_t m_currentInstructionIndex = 0; /**< Index of active instruction. */
};
