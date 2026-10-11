#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

class CCompiler : public kern::ICompiler
{
public:
  /**
  * \brief Constructs a compiler.
  * \param logger Receives diagnostics. A null pointer discards them.
  */
  explicit CCompiler(kern::ILogger *logger);

  bool compile(std::istream &sourceCode, std::ostream &asmCode);

private:
  kern::ILogger *m_logger;
};