#include <catch2/catch_test_macros.hpp>

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "common/LexerToken.h"
#include "comp/Ast.h"
#include "comp/Lower.h"
#include "comp/Parser.h"
#include "logging/Logger.h"
#include "testutil/TestSupport.hpp"

namespace
{

Tok makeTok(LexerToken type, const std::string &text = "")
{
    Tok tok;
    tok.type = type;
    tok.text = text;
    return tok;
}

Ctx makeCtx(const std::vector<Tok> &toks, std::map<std::string, unsigned int> &externs,
            std::map<std::string, FuncInfo> &funcs, std::map<std::string, int> &externUse)
{
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

bool parseAstSource(const std::string &source, std::vector<Tok> &toks, ast::Program &program)
{
    std::istringstream input(source);
    if (!tokenize(input, nullLogger(), toks))
    {
        return false;
    }
    return ast::parseProgram(toks, nullLogger(), program);
}

} // namespace

TEST_CASE("Parser lowers add/mul precedence in order", "[integration][compiler][parser]")
{
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

TEST_CASE("Parser rejects undeclared variable without consuming", "[integration][compiler][parser]")
{
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

TEST_CASE("Parser lowers var declaration statement", "[integration][compiler][parser]")
{
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

TEST_CASE("Compiler tokenize keeps keywords and drops newlines/comments", "[integration][compiler][parser]")
{
    std::istringstream input("func main() {\n// a comment\nvar x;\n}");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks));
    REQUIRE(toks.size() == 9);
    REQUIRE(toks[0].type == LexerToken::Keyword);
    REQUIRE(toks[0].text == "func");
    REQUIRE(toks[1].type == LexerToken::Identifier);
    REQUIRE(toks[1].text == "main");
    for (const auto &tok : toks)
    {
        REQUIRE(tok.type != LexerToken::NewLine);
        REQUIRE(tok.type != LexerToken::Comment);
    }

    std::istringstream words("and or xor not");
    std::vector<Tok> kw;
    REQUIRE(tokenize(words, nullLogger(), kw));
    REQUIRE(kw.size() == 4);
    REQUIRE(kw[0].text == "and");
    REQUIRE(kw[3].text == "not");
    for (const auto &tok : kw)
    {
        REQUIRE(tok.type == LexerToken::Keyword);
    }
}

TEST_CASE("Compiler tokenize rejects invalid characters", "[integration][compiler][parser]")
{
    std::istringstream input("func `() {}");
    std::vector<Tok> toks;
    REQUIRE_FALSE(tokenize(input, nullLogger(), toks));
}

TEST_CASE("Compiler parseProgram collects extern and func signatures", "[integration][compiler][parser]")
{
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

TEST_CASE("Compiler parseProgram rejects duplicate names", "[integration][compiler][parser]")
{
    std::istringstream input("func dup() {\n}\nfunc dup() {\n}\n");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks));
    Program program;
    REQUIRE_FALSE(parseProgram(toks, nullLogger(), program));
}

TEST_CASE("Compiler parseProgram classifies valued and void returns", "[integration][compiler][parser]")
{
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

TEST_CASE("Compiler lowerProgram and emitProgram shape on tiny program", "[integration][compiler][parser]")
{
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

TEST_CASE("Compiler tokenize records token positions", "[integration][compiler][parser]")
{
    std::istringstream input("func main() {\nvar x = 1;\n}\n");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks));
    REQUIRE(toks.size() == 11);
    REQUIRE(toks[0].text == "func");
    REQUIRE(toks[0].pos.line == 1);
    REQUIRE(toks[0].pos.col == 1);
    REQUIRE(toks[1].text == "main");
    REQUIRE(toks[1].pos.line == 1);
    REQUIRE(toks[1].pos.col == 6);
    REQUIRE(toks[5].text == "var");
    REQUIRE(toks[5].pos.line == 2);
    REQUIRE(toks[5].pos.col == 1);
    REQUIRE(toks[7].text == "=");
    REQUIRE(toks[7].pos.line == 2);
    REQUIRE(toks[7].pos.col == 7);
}

TEST_CASE("Diagnostic format renders file line col with a note", "[integration][compiler][parser]")
{
    REQUIRE(formatDiagnostic("test.ks", SrcPos{2, 5}, "Use of undeclared variable.") ==
            "test.ks:2:5: error: Use of undeclared variable.");
    REQUIRE(formatHint("declare it with var or as a parameter") == "note: declare it with var or as a parameter");

    testutil::RecordingLogger logger;
    reportError(&logger, "test.ks", SrcPos{2, 5}, "Use of undeclared variable.",
                "declare it with var or as a parameter");
    REQUIRE(logger.messages().size() == 2);
    REQUIRE(logger.messages()[0] == "test.ks:2:5: error: Use of undeclared variable.");
    REQUIRE(logger.messages()[1] == "note: declare it with var or as a parameter");

    testutil::RecordingLogger plain;
    reportError(&plain, "<input>", SrcPos{1, 1}, "Expected ';'.");
    REQUIRE(plain.messages().size() == 1);
    REQUIRE(plain.messages()[0] == "<input>:1:1: error: Expected ';'.");
}

TEST_CASE("AST builds extern and valued func with spans", "[integration][compiler][parser]")
{
    const std::string source = "extern putc(c);\nfunc main(x) {\nvar r = x + 1;\nreturn r;\n}\n";
    std::vector<Tok> toks;
    ast::Program program;
    REQUIRE(parseAstSource(source, toks, program));

    REQUIRE(program.externs.size() == 1);
    REQUIRE(program.externs[0].name == "putc");
    REQUIRE(program.externs[0].params == std::vector<std::string>{"c"});
    REQUIRE(program.externs[0].pos.line == 1);
    REQUIRE(program.externs[0].pos.col == 1);
    REQUIRE(program.externs[0].end.line == 1);
    REQUIRE(program.externs[0].end.col == 15);

    REQUIRE(program.funcs.size() == 1);
    const ast::Func &func = program.funcs[0];
    REQUIRE(func.name == "main");
    REQUIRE(func.params == std::vector<std::string>{"x"});
    REQUIRE(func.pos.line == 2);
    REQUIRE(func.pos.col == 1);
    REQUIRE(func.end.line == 5);
    REQUIRE(func.end.col == 1);
    REQUIRE(func.hasValuedReturn);
    REQUIRE_FALSE(func.hasBareReturn);
    REQUIRE(func.body.size() == 2);

    const auto *varDecl = std::get_if<ast::Stmt::VarDecl>(&func.body[0].node);
    REQUIRE(varDecl != nullptr);
    REQUIRE(varDecl->name == "r");
    REQUIRE(varDecl->namePos.line == 3);
    REQUIRE(varDecl->namePos.col == 5);
    REQUIRE(func.body[0].pos.line == 3);
    REQUIRE(func.body[0].pos.col == 1);
    REQUIRE(func.body[0].end.line == 3);
    REQUIRE(func.body[0].end.col == 14);
    REQUIRE(varDecl->init != nullptr);
    const auto *add = std::get_if<ast::Expr::Binary>(&varDecl->init->node);
    REQUIRE(add != nullptr);
    REQUIRE(add->op == "+");
    const auto *left = std::get_if<ast::Expr::VarRef>(&add->left->node);
    REQUIRE(left != nullptr);
    REQUIRE(left->name == "x");
    const auto *right = std::get_if<ast::Expr::IntLit>(&add->right->node);
    REQUIRE(right != nullptr);
    REQUIRE(right->text == "1");

    const auto *ret = std::get_if<ast::Stmt::Return>(&func.body[1].node);
    REQUIRE(ret != nullptr);
    REQUIRE(ret->value != nullptr);
    const auto *retVar = std::get_if<ast::Expr::VarRef>(&ret->value->node);
    REQUIRE(retVar != nullptr);
    REQUIRE(retVar->name == "r");
    REQUIRE(func.body[1].pos.line == 4);
    REQUIRE(func.body[1].pos.col == 1);

    // The old string-emitting path still accepts the same program.
    Program oldProgram;
    REQUIRE(parseProgram(toks, nullLogger(), oldProgram));
}

TEST_CASE("AST builds if else while with logic precedence", "[integration][compiler][parser]")
{
    const std::string source = "func main(a) {\nif (a == 1 or a == 2) {\nputc(a);\n} else {\nputc(0);\n}\n"
                               "while (a > 0) {\na = a - 1;\n}\n}\n";
    std::vector<Tok> toks;
    ast::Program program;
    REQUIRE(parseAstSource(source, toks, program));

    REQUIRE(program.funcs.size() == 1);
    const ast::Func &func = program.funcs[0];
    REQUIRE(func.body.size() == 2);

    const auto *branch = std::get_if<ast::Stmt::If>(&func.body[0].node);
    REQUIRE(branch != nullptr);
    REQUIRE(func.body[0].pos.line == 2);
    REQUIRE(func.body[0].pos.col == 1);
    const auto *orExpr = std::get_if<ast::Expr::Binary>(&branch->cond->node);
    REQUIRE(orExpr != nullptr);
    REQUIRE(orExpr->op == "or");
    const auto *first = std::get_if<ast::Expr::Binary>(&orExpr->left->node);
    REQUIRE(first != nullptr);
    REQUIRE(first->op == "==");
    const auto *second = std::get_if<ast::Expr::Binary>(&orExpr->right->node);
    REQUIRE(second != nullptr);
    REQUIRE(second->op == "==");
    REQUIRE(branch->hasElse);
    REQUIRE(branch->thenBranch.size() == 1);
    REQUIRE(branch->elseBranch.size() == 1);
    const auto *thenCall = std::get_if<ast::Stmt::ExprStmt>(&branch->thenBranch[0].node);
    REQUIRE(thenCall != nullptr);
    const auto *thenTarget = std::get_if<ast::Expr::Call>(&thenCall->expr->node);
    REQUIRE(thenTarget != nullptr);
    REQUIRE(thenTarget->name == "putc");
    REQUIRE(thenTarget->args.size() == 1);

    const auto *loop = std::get_if<ast::Stmt::While>(&func.body[1].node);
    REQUIRE(loop != nullptr);
    const auto *cond = std::get_if<ast::Expr::Binary>(&loop->cond->node);
    REQUIRE(cond != nullptr);
    REQUIRE(cond->op == ">");
    REQUIRE(loop->body.size() == 1);
    const auto *assign = std::get_if<ast::Stmt::Assign>(&loop->body[0].node);
    REQUIRE(assign != nullptr);
    REQUIRE(assign->name == "a");
    const auto *sub = std::get_if<ast::Expr::Binary>(&assign->value->node);
    REQUIRE(sub != nullptr);
    REQUIRE(sub->op == "-");
}

TEST_CASE("AST builds strings calls and unary with spans", "[integration][compiler][parser]")
{
    const std::string source = "func double(v) {\nreturn v * 2;\n}\nfunc main {\nvar s = \"hi\";\n"
                               "var r = not double(21) + 1;\ndouble(r);\n}\n";
    std::vector<Tok> toks;
    ast::Program program;
    REQUIRE(parseAstSource(source, toks, program));

    REQUIRE(program.funcs.size() == 2);
    REQUIRE(program.funcs[0].name == "double");
    REQUIRE(program.funcs[0].pos.line == 1);
    REQUIRE(program.funcs[0].pos.col == 1);
    const ast::Func &main = program.funcs[1];
    REQUIRE(main.pos.line == 4);
    REQUIRE(main.pos.col == 1);
    REQUIRE(main.body.size() == 3);

    const auto *strDecl = std::get_if<ast::Stmt::VarDecl>(&main.body[0].node);
    REQUIRE(strDecl != nullptr);
    REQUIRE(strDecl->name == "s");
    const auto *str = std::get_if<ast::Expr::StrLit>(&strDecl->init->node);
    REQUIRE(str != nullptr);
    REQUIRE(str->text == "hi");

    const auto *numDecl = std::get_if<ast::Stmt::VarDecl>(&main.body[1].node);
    REQUIRE(numDecl != nullptr);
    const auto *add = std::get_if<ast::Expr::Binary>(&numDecl->init->node);
    REQUIRE(add != nullptr);
    REQUIRE(add->op == "+");
    const auto *neg = std::get_if<ast::Expr::Unary>(&add->left->node);
    REQUIRE(neg != nullptr);
    REQUIRE(neg->op == "not");
    const auto *call = std::get_if<ast::Expr::Call>(&neg->operand->node);
    REQUIRE(call != nullptr);
    REQUIRE(call->name == "double");
    REQUIRE(call->args.size() == 1);
    const auto *arg = std::get_if<ast::Expr::IntLit>(&call->args[0]->node);
    REQUIRE(arg != nullptr);
    REQUIRE(arg->text == "21");
    const auto *one = std::get_if<ast::Expr::IntLit>(&add->right->node);
    REQUIRE(one != nullptr);

    const auto *callStmt = std::get_if<ast::Stmt::ExprStmt>(&main.body[2].node);
    REQUIRE(callStmt != nullptr);
    const auto *stmtCall = std::get_if<ast::Expr::Call>(&callStmt->expr->node);
    REQUIRE(stmtCall != nullptr);
    REQUIRE(stmtCall->name == "double");
    REQUIRE(stmtCall->args.size() == 1);
    REQUIRE(std::get_if<ast::Expr::VarRef>(&stmtCall->args[0]->node) != nullptr);
}

TEST_CASE("AST rejects malformed input", "[integration][compiler][parser]")
{
    const std::string missingSemi = "func main {\nvar x = 1\n}\n";
    const std::string missingParen = "func main {\nvar x = (1;\n}\n";
    const std::string missingBrace = "func main {\nvar x = 1;\n";
    const std::string noFunc = "extern putc(c);\n";
    const std::string badLiteral = "func main {\nvar x = 4294967296;\n}\n";
    const std::string duplicate = "func main {\n}\nfunc main {\n}\n";
    const std::string mixed = "func main {\nreturn 1;\nreturn;\n}\n";
    for (const std::string *source :
         {&missingSemi, &missingParen, &missingBrace, &noFunc, &badLiteral, &duplicate, &mixed})
    {
        std::vector<Tok> toks;
        ast::Program program;
        REQUIRE_FALSE(parseAstSource(*source, toks, program));
    }
    std::vector<Tok> emptyToks;
    ast::Program emptyProgram;
    REQUIRE_FALSE(parseAstSource("", emptyToks, emptyProgram));
}
