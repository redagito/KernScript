#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

#include "common/LexerToken.h"
#include "comp/Parser.h"
#include "logging/Logger.h"

namespace {

Tok makeTok(LexerToken type, const std::string &text = "") {
  Tok tok;
  tok.type = type;
  tok.text = text;
  return tok;
}

Ctx makeCtx(const std::vector<Tok> &toks, std::map<std::string, unsigned int> &externs,
            std::map<std::string, FuncInfo> &funcs, std::map<std::string, int> &externUse) {
  Ctx ctx;
  ctx.toks = &toks;
  ctx.pos = 0;
  ctx.end = toks.size();
  ctx.externs = &externs;
  ctx.funcs = &funcs;
  ctx.externUse = &externUse;
  ctx.logger = nullLogger();
  return ctx;
}

} // namespace

TEST_CASE("Parser lowers add/mul precedence in order", "[integration][compiler][parser]") {
  const std::vector<Tok> toks = {makeTok(LexerToken::Integer, "1"), makeTok(LexerToken::Plus),
                                 makeTok(LexerToken::Integer, "2"), makeTok(LexerToken::Asterisk),
                                 makeTok(LexerToken::Integer, "3")};
  std::map<std::string, unsigned int> externs;
  std::map<std::string, FuncInfo> funcs;
  std::map<std::string, int> externUse;
  Ctx ctx = makeCtx(toks, externs, funcs, externUse);

  std::vector<std::string> out;
  bool isString = true;
  REQUIRE(parseExpr(ctx, out, isString));
  REQUIRE_FALSE(isString);
  REQUIRE(out == std::vector<std::string>{"pushi 1", "pushi 2", "pushi 3", "mul", "add"});
  REQUIRE(ctx.pos == toks.size());
}

TEST_CASE("Parser rejects undeclared variable without consuming", "[integration][compiler][parser]") {
  const std::vector<Tok> toks = {makeTok(LexerToken::Identifier, "q")};
  std::map<std::string, unsigned int> externs;
  std::map<std::string, FuncInfo> funcs;
  std::map<std::string, int> externUse;
  Ctx ctx = makeCtx(toks, externs, funcs, externUse);

  std::vector<std::string> out;
  bool isString = false;
  REQUIRE_FALSE(parseExpr(ctx, out, isString));
  REQUIRE(ctx.pos == 0);

  const std::vector<Tok> bad = {makeTok(LexerToken::Semicolon, ";")};
  Ctx badCtx = makeCtx(bad, externs, funcs, externUse);
  std::vector<std::string> badOut;
  bool badIsString = false;
  REQUIRE_FALSE(parseExpr(badCtx, badOut, badIsString));
  REQUIRE(badCtx.pos == 0);
}

TEST_CASE("Parser lowers var declaration statement", "[integration][compiler][parser]") {
  const std::vector<Tok> toks = {makeTok(LexerToken::Keyword, "var"), makeTok(LexerToken::Identifier, "r"),
                                 makeTok(LexerToken::Assign),         makeTok(LexerToken::Integer, "1"),
                                 makeTok(LexerToken::Plus),           makeTok(LexerToken::Integer, "2"),
                                 makeTok(LexerToken::Semicolon)};
  std::map<std::string, unsigned int> externs;
  std::map<std::string, FuncInfo> funcs;
  std::map<std::string, int> externUse;
  Ctx ctx = makeCtx(toks, externs, funcs, externUse);

  std::vector<std::string> out;
  REQUIRE(parseStatement(ctx, out));
  REQUIRE(out == std::vector<std::string>{"pushi 1", "pushi 2", "add", "popv r"});
  REQUIRE(ctx.vars.find("r") != ctx.vars.end());
  REQUIRE(ctx.pos == toks.size());
}
