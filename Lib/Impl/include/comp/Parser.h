#pragma once

#include <cstddef>
#include <istream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <kern/ILogger.h>

#include "common/LexerToken.h"

/**
 * \brief Token buffer and lowering context for Compiler::compile.
 *
 * This module holds the parsing/lowering free functions used by
 * Compiler::compile (see Compiler.h) and directly by parser unit tests.
 * Tok is one significant lexer token, FuncInfo is a collected function
 * signature plus body range and return classification, and Ctx is the
 * mutable per-function lowering state (token range, symbol tables,
 * emitted body lines, label counter, logger). Expression lowering is
 * Or > Xor > And > Cmp > Add > Mul > Unary > Primary; statement lowering
 * covers VarDecl, Assign, If/Else, While, Return, and ExprStmt/call forms.
 * tokenize buffers significant tokens from source text, Program is the
 * collected extern/function bookkeeping, and parseProgram fills it by
 * scanning top-level declarations and classifying valued/void returns.
 * SrcPos is a 1-based line/column; every Tok carries the position where
 * its lexeme starts. formatDiagnostic renders the clang-style
 * "<file>:<line>:<col>: error: <message>" line and reportError logs it,
 * followed by a "note: <hint>" line when a hint is given.
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

struct FuncInfo
{
    std::string name;
    std::vector<std::string> params;
    std::size_t bodyBegin = 0;
    std::size_t bodyEnd = 0;
    bool hasValuedReturn = false;
    bool hasBareReturn = false;
};

struct Program
{
    std::map<std::string, unsigned int> externs;
    std::vector<std::pair<std::string, unsigned int>> externOrder;
    std::vector<FuncInfo> funcOrder;
    std::map<std::string, FuncInfo> funcs;
};

bool tokenize(std::istream &sourceCode, kern::ILogger *logger, std::vector<Tok> &toks);
bool topAtEnd(std::size_t pos, const std::vector<Tok> &toks);
LexerToken topPeek(std::size_t pos, std::size_t off, const std::vector<Tok> &toks);
std::string formatDiagnostic(const std::string &sourceName, SrcPos pos, const std::string &message);
std::string formatHint(const std::string &hint);
void reportError(kern::ILogger *logger, const std::string &sourceName, SrcPos pos, const std::string &message,
                 const std::string &hint = "");
bool parseProgram(const std::vector<Tok> &toks, kern::ILogger *logger, Program &program);

struct Ctx
{
    const std::vector<Tok> *toks = nullptr;
    std::size_t pos = 0;
    std::size_t end = 0;
    const std::map<std::string, unsigned int> *externs = nullptr;
    const std::map<std::string, FuncInfo> *funcs = nullptr;
    std::map<std::string, int> *externUse = nullptr; // 1 = value, 2 = statement
    std::map<std::string, unsigned int> vars;
    std::vector<std::string> paramOrder;
    std::vector<std::string> varOrder;
    std::string retTemp;
    std::vector<std::string> body;
    bool funcValued = false;
    bool funcHasBare = false;
    unsigned int labelCounter = 0;
    kern::ILogger *logger = nullptr;
};

bool atEnd(const Ctx &ctx);
LexerToken peekType(const Ctx &ctx);
const std::string &peekText(const Ctx &ctx);
bool expect(Ctx &ctx, LexerToken type, const std::string &what);
bool markExternUse(Ctx &ctx, const std::string &name, bool valueUse);
bool emitPushUint(Ctx &ctx, const std::string &digits, std::vector<std::string> &out);
bool parseCallArgs(Ctx &ctx, std::vector<std::string> &args, unsigned int &count);
bool lowerValueCall(Ctx &ctx, const std::string &name, std::vector<std::string> &out);
bool parseCallStmt(Ctx &ctx, const std::string &name, std::vector<std::string> &out);
bool parsePrimary(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseUnary(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseMul(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseAdd(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseExpr(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseCmp(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseAnd(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseXor(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseOr(Ctx &ctx, std::vector<std::string> &out, bool &isString);
bool parseCondition(Ctx &ctx, std::vector<std::string> &out, std::string &falseJump);
bool parseStmtList(Ctx &ctx, std::vector<std::string> &out);
bool parseIf(Ctx &ctx, std::vector<std::string> &out);
bool parseWhile(Ctx &ctx, std::vector<std::string> &out);
bool parseReturn(Ctx &ctx, std::vector<std::string> &out);
bool parseStatement(Ctx &ctx, std::vector<std::string> &out);
