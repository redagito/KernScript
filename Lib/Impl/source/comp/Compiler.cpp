#include "Compiler.h"

#include "logging/Logger.h"

Compiler::Compiler(kern::ILogger *logger)
    : m_logger(logger != nullptr ? logger : nullLogger())
{
  // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode)
{
  // Not implemented yet. The parser exists but does not produce output.
  return false;
}
