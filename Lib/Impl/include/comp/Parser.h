#pragma once

#include <istream>
#include <string>
#include <vector>

#include <kern/ILogger.h>

#include "common/LexerToken.h"

/**
 * \brief Token buffer for Compiler::compile.
 *
 * This module holds the tokenizing free function used by
 * Compiler::compile (see Compiler.h) and directly by parser unit tests.
 * Tok is one significant lexer token. tokenize buffers significant tokens
 * from source text. SrcPos is a 1-based line/column; every Tok carries the
 * position where its lexeme starts. formatDiagnostic renders the
 * clang-style "<file>:<line>:<col>: error: <message>" line and reportError
 * logs it, followed by a "note: <hint>" line when a hint is given. The
 * syntax tree lives in Ast.h and lowering in Lower.h.
 */

struct SrcPos
{
    unsigned line = 1;
    unsigned col = 1;
};

struct Tok
{
    LexerToken type = LexerToken::Invalid;
    std::string text;
    SrcPos pos;
};

bool tokenize(std::istream &sourceCode, kern::ILogger *logger, std::vector<Tok> &toks);
std::string formatDiagnostic(const std::string &sourceName, SrcPos pos, const std::string &message);
std::string formatHint(const std::string &hint);
void reportError(kern::ILogger *logger, const std::string &sourceName, SrcPos pos, const std::string &message,
                 const std::string &hint = "");
