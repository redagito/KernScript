#pragma once

#include <cstddef>
#include <map>
#include <string>
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
 */

struct Tok {
  LexerToken type = LexerToken::Invalid;
  std::string text;
};

struct FuncInfo {
  std::string name;
  std::vector<std::string> params;
  std::size_t bodyBegin = 0;
  std::size_t bodyEnd = 0;
  bool hasValuedReturn = false;
  bool hasBareReturn = false;
};

struct Ctx {
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
