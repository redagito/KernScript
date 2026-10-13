#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

#include "common/LexerToken.h"
#include "comp/Ast.h"
#include "comp/Lower.h"
#include "comp/Parser.h"
#include "logging/Logger.h"
#include "testutil/TestSupport.hpp"

namespace
{

bool parseAstSource(const std::string &source, std::vector<Tok> &toks, ast::Program &program)
{
    std::istringstream input(source);
    if (!tokenize(input, nullLogger(), toks, "test.ks"))
    {
        return false;
    }
    return ast::parseProgram(toks, nullLogger(), "test.ks", program);
}

bool lowerAstSource(const std::string &source, ast::Program &program, std::vector<Lowered> &lowered)
{
    std::vector<Tok> toks;
    std::istringstream input(source);
    if (!tokenize(input, nullLogger(), toks, "test.ks"))
    {
        return false;
    }
    if (!ast::parseProgram(toks, nullLogger(), "test.ks", program))
    {
        return false;
    }
    return lowerProgram(program, nullLogger(), "test.ks", lowered);
}

} // namespace

TEST_CASE("AST expresses add/mul precedence in order", "[integration][compiler][parser]")
{
    std::vector<Tok> toks;
    ast::Program program;
    REQUIRE(parseAstSource("func main() {\nvar r = 1 + 2 * 3;\n}\n", toks, program));
    REQUIRE(program.funcs.size() == 1);
    REQUIRE(program.funcs[0].body.size() == 1);

    const auto *varDecl = std::get_if<ast::Stmt::VarDecl>(&program.funcs[0].body[0].node);
    REQUIRE(varDecl != nullptr);
    const auto *add = std::get_if<ast::Expr::Binary>(&varDecl->init->node);
    REQUIRE(add != nullptr);
    REQUIRE(add->op == "+");
    REQUIRE(std::get_if<ast::Expr::IntLit>(&add->left->node) != nullptr);
    const auto *mul = std::get_if<ast::Expr::Binary>(&add->right->node);
    REQUIRE(mul != nullptr);
    REQUIRE(mul->op == "*");
}

TEST_CASE("Lower rejects use of undeclared variable", "[integration][compiler][parser]")
{
    ast::Program readProgram;
    std::vector<Lowered> readLowered;
    {
        std::vector<Tok> toks;
        REQUIRE(parseAstSource("func main() {\nvar r = q;\n}\n", toks, readProgram));
        REQUIRE_FALSE(lowerProgram(readProgram, nullLogger(), "test.ks", readLowered));
    }

    ast::Program assignProgram;
    std::vector<Lowered> assignLowered;
    {
        std::vector<Tok> toks;
        REQUIRE(parseAstSource("func main() {\nq = 1;\n}\n", toks, assignProgram));
        REQUIRE_FALSE(lowerProgram(assignProgram, nullLogger(), "test.ks", assignLowered));
    }

    ast::Program badProgram;
    {
        std::vector<Tok> toks;
        REQUIRE(parseAstSource("func main() {\nvar r = 1 +;\n}\n", toks, badProgram) == false);
    }
}

TEST_CASE("Lower lowers var declaration statement", "[integration][compiler][parser]")
{
    ast::Program program;
    std::vector<Lowered> lowered;
    REQUIRE(lowerAstSource("func main() {\nvar r = 1 + 2;\n}\n", program, lowered));
    REQUIRE(lowered.size() == 1);
    REQUIRE(lowered[0].body == std::vector<std::string>{"pushi 1", "pushi 2", "add", "popv r"});
    REQUIRE(lowered[0].vars == std::vector<std::string>{"r"});
}

TEST_CASE("Compiler tokenize keeps keywords and drops newlines/comments", "[integration][compiler][parser]")
{
    std::istringstream input("func main() {\n// a comment\nvar x;\n}");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks, "test.ks"));
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
    REQUIRE(tokenize(words, nullLogger(), kw, "test.ks"));
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
    REQUIRE_FALSE(tokenize(input, nullLogger(), toks, "test.ks"));
}

TEST_CASE("AST collects extern and func signatures", "[integration][compiler][parser]")
{
    std::istringstream input("extern putc(c);\nfunc main(x) {\nputc(x);\n}\nfunc helper() {\n}\n");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks, "test.ks"));
    ast::Program program;
    REQUIRE(ast::parseProgram(toks, nullLogger(), "test.ks", program));
    REQUIRE(program.externs.size() == 1);
    REQUIRE(program.externs[0].name == "putc");
    REQUIRE(program.externs[0].params == std::vector<std::string>{"c"});
    REQUIRE(program.funcs.size() == 2);
    REQUIRE(program.funcs[0].name == "main");
    REQUIRE(program.funcs[0].params == std::vector<std::string>{"x"});
    REQUIRE(program.funcs[1].name == "helper");
    REQUIRE(program.funcs[1].params.empty());
}

TEST_CASE("AST rejects duplicate names", "[integration][compiler][parser]")
{
    std::istringstream input("func dup() {\n}\nfunc dup() {\n}\n");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks, "test.ks"));
    ast::Program program;
    REQUIRE_FALSE(ast::parseProgram(toks, nullLogger(), "test.ks", program));
}

TEST_CASE("AST classifies valued and void returns", "[integration][compiler][parser]")
{
    std::vector<Tok> valuedToks;
    ast::Program valuedProgram;
    REQUIRE(parseAstSource("func v() {\nreturn 1;\n}\n", valuedToks, valuedProgram));
    REQUIRE(valuedProgram.funcs.at(0).hasValuedReturn);
    REQUIRE_FALSE(valuedProgram.funcs.at(0).hasBareReturn);

    std::vector<Tok> bareToks;
    ast::Program bareProgram;
    REQUIRE(parseAstSource("func w() {\nreturn;\n}\n", bareToks, bareProgram));
    REQUIRE_FALSE(bareProgram.funcs.at(0).hasValuedReturn);
    REQUIRE(bareProgram.funcs.at(0).hasBareReturn);

    std::vector<Tok> mixedToks;
    ast::Program mixedProgram;
    REQUIRE_FALSE(parseAstSource("func m() {\nreturn 1;\nreturn;\n}\n", mixedToks, mixedProgram));
}

TEST_CASE("Compiler lowerProgram and emitProgram shape on tiny program", "[integration][compiler][parser]")
{
    std::istringstream input("func main() {\nvar x = 1;\n}\n");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks, "test.ks"));
    ast::Program program;
    REQUIRE(ast::parseProgram(toks, nullLogger(), "test.ks", program));

    std::vector<Lowered> lowered;
    REQUIRE(lowerProgram(program, nullLogger(), "test.ks", lowered));
    REQUIRE(lowered.size() == 1);
    REQUIRE(lowered[0].name == "main");
    REQUIRE(lowered[0].vars == std::vector<std::string>{"x"});
    REQUIRE(lowered[0].body == std::vector<std::string>{"movi x, 1"});

    std::ostringstream asmCode;
    emitProgram(program, lowered, asmCode);
    REQUIRE(asmCode.str() == "func main\n{\nvar x\nmovi x, 1\nret\n}\n");
}

TEST_CASE("Compiler tokenize records token positions", "[integration][compiler][parser]")
{
    std::istringstream input("func main() {\nvar x = 1;\n}\n");
    std::vector<Tok> toks;
    REQUIRE(tokenize(input, nullLogger(), toks, "test.ks"));
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

    // Lowering the same tree emits the expected body lines.
    std::vector<Lowered> lowered;
    REQUIRE(lowerProgram(program, nullLogger(), "test.ks", lowered));
    REQUIRE(lowered.size() == 1);
    REQUIRE(lowered[0].body == std::vector<std::string>{"pushv x", "pushi 1", "add", "popv r", "retv r"});
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
    const std::string source = "func double(v) {\nreturn v * 2;\n}\nfunc main() {\nvar s = \"hi\";\n"
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
    const std::string missingSemi = "func main() {\nvar x = 1\n}\n";
    const std::string missingParen = "func main() {\nvar x = (1;\n}\n";
    const std::string missingBrace = "func main() {\nvar x = 1;\n";
    const std::string noFunc = "extern putc(c);\n";
    const std::string badLiteral = "func main() {\nvar x = 4294967296;\n}\n";
    const std::string duplicate = "func main() {\n}\nfunc main() {\n}\n";
    const std::string mixed = "func main() {\nreturn 1;\nreturn;\n}\n";
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

namespace
{

std::vector<std::string> compileWithName(const std::string &source, const std::string &sourceName, bool &ok)
{
    testutil::RecordingLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    std::stringstream input(source);
    std::stringstream asmCode;
    ok = compiler->compile(input, asmCode, sourceName);
    return logger.messages();
}

} // namespace

TEST_CASE("Diagnostic reports undeclared name with position and hint", "[integration][compiler][diagnostic]")
{
    const std::string source = "extern println(x);\nfunc main() {\n  println(q);\n}\n";
    bool ok = true;
    const std::vector<std::string> messages = compileWithName(source, "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:3:11: error: Use of undeclared variable.");
    REQUIRE(messages[1] == "note: declare it with var or as a parameter");
}

TEST_CASE("Diagnostic reports arity with position and hint", "[integration][compiler][diagnostic]")
{
    const std::string source = "extern println(x);\nfunc double(v) {\n  return v * 2;\n}\nfunc main() {\n"
                               "  println(double(1, 2));\n}\n";
    bool ok = true;
    const std::vector<std::string> messages = compileWithName(source, "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:6:11: error: Wrong argument count.");
    REQUIRE(messages[1] == "note: declared 1 parameters here");
}

TEST_CASE("Diagnostic reports mixed returns with position and hint", "[integration][compiler][diagnostic]")
{
    const std::string source = "extern println(x);\nfunc f(v) {\n  if (v == 1) {\n    return 1;\n  }\n  return;\n}\n"
                               "func main() {\n  println(f(2));\n}\n";
    bool ok = true;
    const std::vector<std::string> messages = compileWithName(source, "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:6:3: error: Mixed valued and void returns.");
    REQUIRE(messages[1] == "note: use one return form in each function");
}

TEST_CASE("Diagnostic reports bad literal with position and hint", "[integration][compiler][diagnostic]")
{
    const std::string source = "extern println(x);\nfunc main() {\n  var x = 4294967296;\n  println(x);\n}\n";
    bool ok = true;
    const std::vector<std::string> messages = compileWithName(source, "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:3:11: error: Integer literal out of range.");
    REQUIRE(messages[1] == "note: use 0 to 4294967295");
}

TEST_CASE("Diagnostic reports expected token with position and no hint", "[integration][compiler][diagnostic]")
{
    const std::string source = "func main() {\nvar x = 1\n}\n";
    bool ok = true;
    const std::vector<std::string> messages = compileWithName(source, "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0] == "test.ks:3:1: error: Expected ';'.");
}

TEST_CASE("Diagnostic uses the input name without an explicit source name", "[integration][compiler][diagnostic]")
{
    const std::string source = "extern println(x);\nfunc main() {\n  println(q);\n}\n";
    testutil::RecordingLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
    REQUIRE_FALSE(logger.messages().empty());
    REQUIRE(logger.messages()[0] == "<input>:3:11: error: Use of undeclared variable.");
}

namespace
{

std::string makeNestedSource(int depth)
{
    std::string source = "extern println(x);\nfunc main() {\n";
    for (int i = 0; i < depth; ++i)
    {
        source += "if (1 == 1) {\nprintln(1);\n} else {\n";
    }
    source += "println(0);\n";
    for (int i = 0; i < depth; ++i)
    {
        source += "}\n";
    }
    return source + "}\n";
}

} // namespace

TEST_CASE("Parser rejects deeply nested blocks with a clean error", "[integration][compiler][parser]")
{
    bool ok = true;
    const std::vector<std::string> messages = compileWithName(makeNestedSource(400), "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE_FALSE(messages.empty());
    REQUIRE(messages[0].find("error: Nesting too deep.") != std::string::npos);
}

TEST_CASE("Parser still accepts moderate nesting", "[integration][compiler][parser]")
{
    bool ok = true;
    const std::vector<std::string> messages = compileWithName(makeNestedSource(100), "test.ks", ok);
    REQUIRE(ok);
}
