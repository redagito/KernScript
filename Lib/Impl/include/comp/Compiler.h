#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

class Compiler : public kern::ICompiler
{
public:
    /**
     * \brief Constructs a compiler.
     * \param logger Receives diagnostics. A null pointer discards them.
     *
     * NOTE: slices 1+2+3+4+5+6+7. compile() accepts ExternDecl* followed by
     * FuncDecl* where each function is 'func' Ident ['(' params ')']
     * '{' Stmt* '}' and Stmt is VarDecl | Assign | If (with optional
     * Else) | While | Return | ExprStmt. Calls lower as expressions
     * (left-to-right args, 'call' leaves the return value) or statements
     * (valued script calls add 'pop'; extern calls stay a bare 'calle').
     * A function is valued iff it contains 'return expr;', void otherwise;
     * mixing both, using a void call as a value, a wrong argument count,
     * an undeclared name, or inconsistent extern value/statement use all
     * return false. Signatures and classification are collected before any
     * body lowers, so forward references resolve. Conditions are full logic
     * expressions (Or > Xor > And > Cmp, not at Unary level); comparisons
     * lower to 1/0 values via a true-jump over pushi 0 / pushi 1, logic ops
     * emit and/or/xor and not negates in place, and if/while tests the value
     * against 0 with je, with je/jne comparing strings by value. If uses a
     * unique per-if else/end label; while emits start label, condition,
     * inverse jump to end, body, jmp back to start with unique per-loop
     * labels. Unary minus lowers as 0 - x. String and float literals lower
     * to pushs/pushf; arithmetic on a statically known string (literals,
     * including parenthesized) is a compile error, while string operands
     * held in variables still fail at runtime. Anything else
     * returns false. Diagnostics use the clang-style
     * "<file>:<line>:<col>: error: <message>" form with a "note: <hint>"
     * follow-up where a hint exists; the two-argument compile() reports
     * "<input>" as the file name.
     */
    explicit Compiler(kern::ILogger *logger);

    bool compile(std::istream &sourceCode, std::ostream &asmCode) override;
    bool compile(std::istream &sourceCode, std::ostream &asmCode, const std::string &sourceName) override;

private:
    kern::ILogger *m_logger;
};