#include "comp/Compiler.h"

#include <vector>

#include "comp/Ast.h"
#include "comp/Parser.h"
#include "logging/Logger.h"

#include "comp/Lower.h"

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger())
{
    // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode)
{
    return compile(sourceCode, asmCode, "<input>");
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode, const std::string &sourceName)
{
    // S1: the name is accepted for diagnostics but not yet threaded;
    // positions are recorded on tokens but unused elsewhere yet.
    (void)sourceName;
    // Orchestration only: tokenize, parse to an AST, lower, emit. The
    // language contract lives on the ctor (see Compiler.h); each phase
    // lives in its own module (see Ast.h, Parser.h and Lower.h).
    std::vector<Tok> toks;
    if (!tokenize(sourceCode, m_logger, toks))
    {
        return false;
    }
    ast::Program program;
    if (!ast::parseProgram(toks, m_logger, program))
    {
        return false;
    }
    std::vector<Lowered> lowered;
    if (!lowerProgram(program, m_logger, lowered))
    {
        return false;
    }
    emitProgram(program, lowered, asmCode);
    return true;
}
