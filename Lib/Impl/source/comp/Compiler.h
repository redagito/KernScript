#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

class Compiler : public kern::ICompiler {
public:
  /**
   * \brief Constructs a compiler.
   * \param logger Receives diagnostics. A null pointer discards them.
   *
   * NOTE: slice 0 only. compile() accepts one 'func' Ident '{' '}'
   * declaration and emits assembly with an explicit ret; anything else
   * returns false. The Parser module remains the future frontend (see
   * Parser.h) for later slices.
   */
  explicit Compiler(kern::ILogger *logger);

  bool compile(std::istream &sourceCode, std::ostream &asmCode);

private:
  kern::ILogger *m_logger;
};