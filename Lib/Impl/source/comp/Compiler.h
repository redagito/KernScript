#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

class Compiler : public kern::ICompiler {
public:
  /**
   * \brief Constructs a compiler.
   * \param logger Receives diagnostics. A null pointer discards them.
   *
   * NOTE: slice 1 only. compile() accepts ExternDecl* followed by one
   * 'func' Ident '{' CallStmt* '}' where each statement is a call to a
   * declared extern with integer literal arguments, and emits assembly
   * with 'extern <name> <count>' plus pushi/pushu and calle. Anything
   * else returns false. The Parser module remains the future frontend
   * (see Parser.h) for later slices.
   */
  explicit Compiler(kern::ILogger *logger);

  bool compile(std::istream &sourceCode, std::ostream &asmCode);

private:
  kern::ILogger *m_logger;
};