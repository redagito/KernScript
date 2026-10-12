#include "comp/Compiler.h"

#include <vector>

#include "comp/Parser.h"
#include "logging/Logger.h"

#include "Lower.h"

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger()) {
  // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode) {
  // Orchestration only: tokenize, scan/classify, lower, emit. The language
  // contract lives on the ctor (see Compiler.h); each phase lives in its
  // own module (see Parser.h and Lower.h).
  std::vector<Tok> toks;
  if (!tokenize(sourceCode, m_logger, toks)) {
    return false;
  }
  Program program;
  if (!parseProgram(toks, m_logger, program)) {
    return false;
  }
  std::vector<Lowered> lowered;
  if (!lowerProgram(toks, program, m_logger, lowered)) {
    return false;
  }
  emitProgram(program, lowered, asmCode);
  return true;
}
