#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

class Compiler : public kern::ICompiler {
public:
  /**
   * \brief Constructs a compiler.
   * \param logger Receives diagnostics. A null pointer discards them.
   *
   * NOTE: slices 1+2+3+4+5+6. compile() accepts ExternDecl* followed by
   * FuncDecl* where each function is 'func' Ident ['(' params ')']
   * '{' Stmt* '}' and Stmt is VarDecl | Assign | If (with optional
   * Else) | While | Return | ExprStmt. Calls lower as expressions
   * (left-to-right args, 'call' leaves the return value) or statements
   * (valued script calls add 'pop'; extern calls stay a bare 'calle').
   * A function is valued iff it contains 'return expr;', void otherwise;
   * mixing both, using a void call as a value, a wrong argument count,
   * an undeclared name, or inconsistent extern value/statement use all
   * return false. Signatures and classification are collected before any
   * body lowers, so forward references resolve. Conditions are single
   * CmpExpr comparisons; they emit left/right pushes plus the inverse
   * conditional jump, with je/jne comparing strings by value. If uses a
   * unique per-if else/end label; while emits start label, condition,
   * inverse jump to end, body, jmp back to start with unique per-loop
   * labels. Unary minus lowers as 0 - x. String and float literals lower
   * to pushs/pushf; arithmetic on a statically known string (literals,
   * including parenthesized) is a compile error, while string operands
   * held in variables still fail at runtime. Anything else
   * returns false. The Parser module remains the future frontend
   * (see Parser.h) for later slices.
   */
  explicit Compiler(kern::ILogger *logger);

  bool compile(std::istream &sourceCode, std::ostream &asmCode);

private:
  kern::ILogger *m_logger;
};