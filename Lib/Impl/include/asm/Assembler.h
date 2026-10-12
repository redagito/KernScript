#pragma once

#include <cstdint>
#include <list>
#include <string>
#include <tuple>

#include "common/Lexer.h"
#include "kern/IAssembler.h"

#include "common/ExternFunction.h"
#include "common/Instruction.h"
#include "common/ScriptHeader.h"

class Assembler : public kern::IAssembler {
public:
  /**
   * \brief Constructs an assembler.
   * \param logger Receives diagnostics. A null pointer discards them.
   */
  explicit Assembler(kern::ILogger *logger);

  bool assemble(std::istream &asmCode, std::ostream &byteCode);

protected:
  bool parseFunction(std::istream &stream);
  bool parseExternFunction(std::istream &stream);
  bool parseModule(std::istream &stream);

  bool isFunction(const std::string &name);
  bool getFunctionId(const std::string &name, int32_t &id);

  bool isExternFunction(const std::string &name);
  bool getExternFunctionId(const std::string &name, int32_t &id);

  /**
   * \brief Serialization.
   */
  bool serializeStrings(std::ostream &stream) const;
  bool serializeExternFunctions(std::ostream &stream) const;
  bool serializeFunctions(std::ostream &stream) const;

private:
  uint32_t addString(const std::string &str);

  /**
   * \brief Represents an assembled script function.
   */
  struct Function {
    std::string name;                    /**< Function name. */
    uint32_t stackSize = 0;              /**< Stack size needed for local variables (includes passed
                                            parameters). */
    uint32_t parameterSize = 0;          /**< Stack size needed for passed parameters. */
    std::list<Instruction> instructions; /**< Assembled instructions. */
  };

  ScriptHeader m_header;                       /**< Assembled script header data. */
  std::list<Function> m_functions;             /**< Assembled script functions. */
  std::list<ExternFunction> m_externFunctions; /**< Extern function declarations. */
  std::list<std::string> m_strings;            /**< String constants. */
  // Calls resolved after the full parse, so callees may be declared later in
  // the script. Same deferred pattern as the per-function jump lists.
  std::list<std::tuple<Instruction *, std::string>> m_unresolvedCalls;
  std::list<std::tuple<Instruction *, std::string>> m_unresolvedExternCalls;
  Lexer m_lexer;                     /**< Internal lexer. */
  kern::ILogger *m_logger = nullptr; /**< Receives diagnostics. */
};
