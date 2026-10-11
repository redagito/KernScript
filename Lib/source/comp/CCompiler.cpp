#include "CCompiler.h"

#include "logging/CLogger.h"

CCompiler::CCompiler(kern::ILogger *logger)
    : m_logger(logger != nullptr ? logger : nullLogger())
{
  // Empty
}

bool CCompiler::compile(std::istream &sourceCode, std::ostream &asmCode)
{
  // Not implemented yet. The parser exists but does not produce output.
  return false;
}
