#include <catch2/catch_test_macros.hpp>

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../../../Lib/Impl/source/comp/Lower.h"
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

TEST_CASE("Compiler tokenize keeps keywords and drops newlines/comments", "[integration][compiler][parser]") {
  std::istringstream input("func main() {\n// a comment\nvar x;\n}");
  std::vector<Tok> toks;
  REQUIRE(tokenize(input, nullLogger(), toks));
  REQUIRE(toks.size() == 9);
  REQUIRE(toks[0].type == LexerToken::Keyword);
  REQUIRE(toks[0].text == "func");
  REQUIRE(toks[1].type == LexerToken::Identifier);
  REQUIRE(toks[1].text == "main");
  for (const auto &tok : toks) {
    REQUIRE(tok.type != LexerToken::NewLine);
    REQUIRE(tok.type != LexerToken::Comment);
  }

  std::istringstream words("and or xor not");
  std::vector<Tok> kw;
  REQUIRE(tokenize(words, nullLogger(), kw));
  REQUIRE(kw.size() == 4);
  REQUIRE(kw[0].text == "and");
  REQUIRE(kw[3].text == "not");
  for (const auto &tok : kw) {
    REQUIRE(tok.type == LexerToken::Keyword);
  }
}

TEST_CASE("Compiler tokenize rejects invalid characters", "[integration][compiler][parser]") {
  std::istringstream input("func `() {}");
  std::vector<Tok> toks;
  REQUIRE_FALSE(tokenize(input, nullLogger(), toks));
}

TEST_CASE("Compiler parseProgram collects extern and func signatures", "[integration][compiler][parser]") {
  std::istringstream input("extern putc(c);\nfunc main(x) {\nputc(x);\n}\nfunc helper() {\n}\n");
  std::vector<Tok> toks;
  REQUIRE(tokenize(input, nullLogger(), toks));
  Program program;
  REQUIRE(parseProgram(toks, nullLogger(), program));
  REQUIRE(program.externs.size() == 1);
  REQUIRE(program.externs.at("putc") == 1);
  REQUIRE(program.externOrder.size() == 1);
  REQUIRE(program.externOrder[0].first == "putc");
  REQUIRE(program.funcOrder.size() == 2);
  REQUIRE(program.funcs.find("main") != program.funcs.end());
  REQUIRE(program.funcs.at("main").params == std::vector<std::string>{"x"});
  REQUIRE(program.funcs.find("helper") != program.funcs.end());
}

TEST_CASE("Compiler parseProgram rejects duplicate names", "[integration][compiler][parser]") {
  std::istringstream input("func dup() {\n}\nfunc dup() {\n}\n");
  std::vector<Tok> toks;
  REQUIRE(tokenize(input, nullLogger(), toks));
  Program program;
  REQUIRE_FALSE(parseProgram(toks, nullLogger(), program));
}

TEST_CASE("Compiler parseProgram classifies valued and void returns", "[integration][compiler][parser]") {
  std::istringstream valued("func v() {\nreturn 1;\n}\n");
  std::vector<Tok> valuedToks;
  REQUIRE(tokenize(valued, nullLogger(), valuedToks));
  Program valuedProgram;
  REQUIRE(parseProgram(valuedToks, nullLogger(), valuedProgram));
  REQUIRE(valuedProgram.funcs.at("v").hasValuedReturn);
  REQUIRE_FALSE(valuedProgram.funcs.at("v").hasBareReturn);

  std::istringstream bare("func w() {\nreturn;\n}\n");
  std::vector<Tok> bareToks;
  REQUIRE(tokenize(bare, nullLogger(), bareToks));
  Program bareProgram;
  REQUIRE(parseProgram(bareToks, nullLogger(), bareProgram));
  REQUIRE_FALSE(bareProgram.funcs.at("w").hasValuedReturn);
  REQUIRE(bareProgram.funcs.at("w").hasBareReturn);

  std::istringstream mixed("func m() {\nreturn 1;\nreturn;\n}\n");
  std::vector<Tok> mixedToks;
  REQUIRE(tokenize(mixed, nullLogger(), mixedToks));
  Program mixedProgram;
  REQUIRE_FALSE(parseProgram(mixedToks, nullLogger(), mixedProgram));
}

TEST_CASE("Compiler lowerProgram and emitProgram shape on tiny program", "[integration][compiler][parser]") {
  std::istringstream input("func main() {\nvar x = 1;\n}\n");
  std::vector<Tok> toks;
  REQUIRE(tokenize(input, nullLogger(), toks));
  Program program;
  REQUIRE(parseProgram(toks, nullLogger(), program));

  std::vector<Lowered> lowered;
  REQUIRE(lowerProgram(toks, program, nullLogger(), lowered));
  REQUIRE(lowered.size() == 1);
  REQUIRE(lowered[0].name == "main");
  REQUIRE(lowered[0].vars == std::vector<std::string>{"x"});
  REQUIRE(lowered[0].body == std::vector<std::string>{"pushi 1", "popv x"});

  std::ostringstream asmCode;
  emitProgram(program, lowered, asmCode);
  REQUIRE(asmCode.str() == "func main\n{\nvar x\npushi 1\npopv x\nret\n}\n");
}
