#pragma once

#include <istream>

#include <kern/ILogger.h>

#include "common/Lexer.h"

class Parser
{
public:
  /**
  * \brief Constructs a parser.
  * \param logger Receives diagnostics. A null pointer discards them.
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