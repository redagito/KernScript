#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

class Compiler : public kern::ICompiler {
public:
  /**
   * \brief Constructs a compiler.
   * \param logger Receives diagnostics. A null pointer discards them.
   *
   * NOTE: slices 1+2+3. compile() accepts ExternDecl* followed by one
   * 'func' Ident '{' Stmt* '}' where Stmt is VarDecl | Assign | If
   * (with optional Else) | extern call with expression arguments
   * (Add/Mul/Unary/Primary, parens, int/float/string literals,
   * variables). If conditions are single CmpExpr comparisons; they emit
   * left/right pushes plus the inverse conditional jump to a unique
   * per-if else/end label. Unary minus lowers as 0 - x. Anything else
   * returns false. The Parser module remains the future frontend
   * (see Parser.h) for later slices.
   */
  explicit Compiler(kern::ILogger *logger);

  bool compile(std::istream &sourceCode, std::ostream &asmCode);

private:
  kern::ILogger *m_logger;
};