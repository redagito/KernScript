#pragma once

#include <istream>

#include <kern/ILogger.h>

#include "common/Lexer.h"

class Parser {
public:
  /**
   * \brief Constructs a parser.
   * \param logger Receives diagnostics. A null pointer discards them.
   *
   * NOTE: explicitly unimplemented stub. parse() dispatches top-level
   * func/extern keywords, but every sub-parser (parseFunction,
   * parseExternFunction, parseIf/Else/While/Return/Block, and
   * parseVariableDeclaration) returns false, so no non-trivial source
   * parses. The class is kept as Compiler's future frontend alongside
   * Compiler.h, which the public kern::createCompiler() factory
   * requires; removing the module would break that factory while
   * saving nothing at runtime. Implementing a parser is out of scope
   * here.
   */
  explicit Parser(kern::ILogger *logger);

  bool parse(std::istream &sourceCode);

protected:
  bool parseVariableDeclaration(std::istream &stream);
  bool parseFunction(std::istream &stream);
  bool parseExternFunction(std::istream &stream);

  bool parseIf(std::istream &stream);
  bool parseElse(std::istream &stream);
  bool parseWhile(std::istream &stream);
  bool parseReturn(std::istream &stream);

  bool parseBlock(std::istream &stream);

private:
  unsigned int m_currentLineNumber = 1;
  unsigned int m_currentLineIndex = 0;
  Lexer m_lexer;
  kern::ILogger *m_logger;
};