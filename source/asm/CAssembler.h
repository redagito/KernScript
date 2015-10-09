#pragma once

#include <list>
#include <string>
#include <cstdint>

#include "kern/IAssembler.h"
#include "common/CLexer.h"

#include "common/SScriptHeader.h"
#include "common/SExternFunction.h"
#include "common/SInstruction.h"

class CAssembler : public kern::IAssembler
{
public:
  CAssembler();
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
  /**
  * \brief Represents an assembled script function.
  */
  struct SFunction
  {
    std::string name;       /**< Function name. */
    uint32_t stackSize = 0; /**< Stack size needed for local variables. */
    std::list<SInstruction> instructions; /**< Assembled instructions. */
  };

  SScriptHeader m_header;           /**< Assembled script header data. */
  std::list<SFunction> m_functions; /**< Assembled script functions. */
  std::list<SExternFunction>
      m_externFunctions;            /**< Extern function declarations. */
  std::list<std::string> m_strings; /**< String constants. */
  CLexer m_lexer;                   /**< Internal lexer. */
};