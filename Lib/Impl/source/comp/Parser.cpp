#include "comp/Parser.h"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/Lexer.h"

#include "comp/Ast.h"

bool tokenize(std::istream &sourceCode, kern::ILogger *logger, std::vector<Tok> &toks, const std::string &sourceName)
{
    Lexer lexer(logger);
    lexer.setSourceName(sourceName);
    lexer.addKeyword("func");
    lexer.addKeyword("extern");
    lexer.addKeyword("var");
    lexer.addKeyword("if");
    lexer.addKeyword("else");
    lexer.addKeyword("while");
    lexer.addKeyword("return");
    // Reserved logic operators: recognized so misuse fails with a clean
    // keyword error instead of parsing as an identifier.
    lexer.addKeyword("and");
    lexer.addKeyword("or");
    lexer.addKeyword("xor");
    lexer.addKeyword("not");

    // Buffer significant tokens for lookahead.
    toks.clear();
    while (true)
    {
        lexer.lex(sourceCode);
        const LexerToken type = lexer.getToken();
        if (type == LexerToken::End)
        {
            break;
        }
        if (type == LexerToken::Invalid)
        {
            SrcPos pos;
            pos.line = lexer.getLine();
            pos.col = lexer.getColumn();
            reportError(logger, sourceName, pos, "Invalid source.");
            return false;
        }
        if (type == LexerToken::NewLine || type == LexerToken::Comment)
        {
            continue;
        }
        Tok tok;
        tok.type = type;
        tok.text = lexer.getLexeme();
        tok.pos.line = lexer.getLine();
        tok.pos.col = lexer.getColumn();
        toks.push_back(tok);
    }
    return true;
}

std::string formatDiagnostic(const std::string &sourceName, SrcPos pos, const std::string &message)
{
    return sourceName + ":" + std::to_string(pos.line) + ":" + std::to_string(pos.col) + ": error: " + message;
}

std::string formatHint(const std::string &hint) { return "note: " + hint; }

void reportError(kern::ILogger *logger, const std::string &sourceName, SrcPos pos, const std::string &message,
                 const std::string &hint)
{
    logger->error("{}", formatDiagnostic(sourceName, pos, message));
    if (!hint.empty())
    {
        logger->error("{}", formatHint(hint));
    }
}

namespace
{

// Cursor for the syntax-only AST build. It carries no symbol tables:
// name resolution runs on the finished tree during lowering.
struct AstBuild
{
    const std::vector<Tok> *toks = nullptr;
    std::size_t pos = 0;
    kern::ILogger *logger = nullptr;
    std::string sourceName;
};

// Position of the current token, the last token at end of input, or 1:1
// for empty input. Fail-fast keeps only the first error, so pointing an
// end-of-input expectation at the last token is enough to locate it.
SrcPos astHere(const AstBuild &build)
{
    if (build.pos < build.toks->size())
    {
        return (*build.toks)[build.pos].pos;
    }
    if (!build.toks->empty())
    {
        return build.toks->back().pos;
    }
    return SrcPos{};
}

void astError(AstBuild &build, const std::string &message, const std::string &hint = "")
{
    reportError(build.logger, build.sourceName, astHere(build), message, hint);
}

bool astAtEnd(const AstBuild &build) { return build.pos >= build.toks->size(); }

LexerToken astPeek(const AstBuild &build, std::size_t off = 0)
{
    if (build.pos + off >= build.toks->size())
    {
        return LexerToken::End;
    }
    return (*build.toks)[build.pos + off].type;
}

const Tok &astTok(const AstBuild &build, std::size_t off = 0) { return (*build.toks)[build.pos + off]; }

bool astExpect(AstBuild &build, LexerToken type, const std::string &what)
{
    if (astPeek(build) != type)
    {
        astError(build, "Expected " + what + ".");
        return false;
    }
    ++build.pos;
    return true;
}

bool astKeywordIs(const AstBuild &build, const std::string &word)
{
    return astPeek(build) == LexerToken::Keyword && astTok(build).text == word;
}

// Builds one integer literal node, splitting signed/unsigned at 2147483647
// exactly like the lowering pushi/pushu split. Anything above 4294967295 is
// a compile error per the lexical section of the spec.
bool makeIntNode(AstBuild &build, const Tok &tok, std::unique_ptr<ast::Expr> &expr)
{
    unsigned long long value = 0;
    for (char c : tok.text)
    {
        if (c < '0' || c > '9')
        {
            reportError(build.logger, build.sourceName, tok.pos, "Invalid integer literal.");
            return false;
        }
        value = value * 10 + static_cast<unsigned long long>(c - '0');
        if (value > 4294967295ULL)
        {
            reportError(build.logger, build.sourceName, tok.pos, "Integer literal out of range.",
                        "use 0 to 4294967295");
            return false;
        }
    }
    auto node = std::make_unique<ast::Expr>();
    node->pos = tok.pos;
    node->end = tok.pos;
    if (value <= 2147483647ULL)
    {
        node->node = ast::Expr::IntLit{tok.text};
    }
    else
    {
        node->node = ast::Expr::UIntLit{tok.text};
    }
    expr = std::move(node);
    return true;
}

bool makeFloatNode(AstBuild &build, const Tok &tok, std::unique_ptr<ast::Expr> &expr)
{
    errno = 0;
    char *end = nullptr;
    const float value = std::strtof(tok.text.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(value))
    {
        reportError(build.logger, build.sourceName, tok.pos, "Float literal out of range.", "use a 32-bit float");
        return false;
    }
    auto node = std::make_unique<ast::Expr>();
    node->node = ast::Expr::FloatLit{tok.text};
    node->pos = tok.pos;
    node->end = tok.pos;
    expr = std::move(node);
    return true;
}

bool parseAstExpr(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstOr(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstXor(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstAnd(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstCmp(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstAdd(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstMul(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstUnary(AstBuild &build, std::unique_ptr<ast::Expr> &expr);
bool parseAstPrimary(AstBuild &build, std::unique_ptr<ast::Expr> &expr);

std::unique_ptr<ast::Expr> makeBinary(std::string op, std::unique_ptr<ast::Expr> left, std::unique_ptr<ast::Expr> right)
{
    auto node = std::make_unique<ast::Expr>();
    node->pos = left->pos;
    node->end = right->end;
    node->node = ast::Expr::Binary{std::move(op), std::move(left), std::move(right)};
    return node;
}

// Parses '(' [Expr (',' Expr)*] ')' with the cursor on '('.
bool parseAstCallArgs(AstBuild &build, std::vector<std::unique_ptr<ast::Expr>> &args)
{
    if (!astExpect(build, LexerToken::OpenParenthesis, "'('"))
    {
        return false;
    }
    args.clear();
    if (astPeek(build) != LexerToken::CloseParenthesis)
    {
        while (true)
        {
            std::unique_ptr<ast::Expr> one;
            if (!parseAstExpr(build, one))
            {
                return false;
            }
            args.push_back(std::move(one));
            if (astPeek(build) == LexerToken::Comma)
            {
                ++build.pos;
                continue;
            }
            break;
        }
    }
    return astExpect(build, LexerToken::CloseParenthesis, "')'");
}

bool parseAstPrimary(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (astAtEnd(build))
    {
        astError(build, "Expected an expression.");
        return false;
    }
    const Tok tok = astTok(build);
    if (tok.type == LexerToken::Integer)
    {
        ++build.pos;
        return makeIntNode(build, tok, expr);
    }
    if (tok.type == LexerToken::Float)
    {
        ++build.pos;
        return makeFloatNode(build, tok, expr);
    }
    if (tok.type == LexerToken::String)
    {
        ++build.pos;
        auto node = std::make_unique<ast::Expr>();
        node->node = ast::Expr::StrLit{tok.text};
        node->pos = tok.pos;
        node->end = tok.pos;
        expr = std::move(node);
        return true;
    }
    if (tok.type == LexerToken::Identifier)
    {
        if (astPeek(build, 1) == LexerToken::OpenParenthesis)
        {
            const SrcPos namePos = tok.pos;
            const std::string name = tok.text;
            ++build.pos;
            std::vector<std::unique_ptr<ast::Expr>> args;
            if (!parseAstCallArgs(build, args))
            {
                return false;
            }
            auto node = std::make_unique<ast::Expr>();
            node->pos = namePos;
            node->end = (*build.toks)[build.pos - 1].pos;
            node->node = ast::Expr::Call{name, namePos, std::move(args)};
            expr = std::move(node);
            return true;
        }
        ++build.pos;
        auto node = std::make_unique<ast::Expr>();
        node->node = ast::Expr::VarRef{tok.text};
        node->pos = tok.pos;
        node->end = tok.pos;
        expr = std::move(node);
        return true;
    }
    if (tok.type == LexerToken::OpenParenthesis)
    {
        ++build.pos;
        if (!parseAstExpr(build, expr))
        {
            return false;
        }
        return astExpect(build, LexerToken::CloseParenthesis, "')'");
    }
    astError(build, "Expected an expression.");
    return false;
}

bool parseAstUnary(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (astPeek(build) == LexerToken::Dash)
    {
        const SrcPos opPos = astTok(build).pos;
        ++build.pos;
        std::unique_ptr<ast::Expr> operand;
        if (!parseAstUnary(build, operand))
        {
            return false;
        }
        auto node = std::make_unique<ast::Expr>();
        node->pos = opPos;
        node->end = operand->end;
        node->node = ast::Expr::Unary{"-", std::move(operand)};
        expr = std::move(node);
        return true;
    }
    if (astKeywordIs(build, "not"))
    {
        const SrcPos opPos = astTok(build).pos;
        ++build.pos;
        std::unique_ptr<ast::Expr> operand;
        if (!parseAstUnary(build, operand))
        {
            return false;
        }
        auto node = std::make_unique<ast::Expr>();
        node->pos = opPos;
        node->end = operand->end;
        node->node = ast::Expr::Unary{"not", std::move(operand)};
        expr = std::move(node);
        return true;
    }
    return parseAstPrimary(build, expr);
}

bool parseAstMul(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (!parseAstUnary(build, expr))
    {
        return false;
    }
    while (astPeek(build) == LexerToken::Asterisk || astPeek(build) == LexerToken::Slash)
    {
        const bool isMul = astPeek(build) == LexerToken::Asterisk;
        ++build.pos;
        std::unique_ptr<ast::Expr> rhs;
        if (!parseAstUnary(build, rhs))
        {
            return false;
        }
        expr = makeBinary(isMul ? "*" : "/", std::move(expr), std::move(rhs));
    }
    return true;
}

bool parseAstAdd(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (!parseAstMul(build, expr))
    {
        return false;
    }
    while (astPeek(build) == LexerToken::Plus || astPeek(build) == LexerToken::Dash)
    {
        const bool isAdd = astPeek(build) == LexerToken::Plus;
        ++build.pos;
        std::unique_ptr<ast::Expr> rhs;
        if (!parseAstMul(build, rhs))
        {
            return false;
        }
        expr = makeBinary(isAdd ? "+" : "-", std::move(expr), std::move(rhs));
    }
    return true;
}

bool parseAstCmp(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (!parseAstAdd(build, expr))
    {
        return false;
    }
    std::string op;
    switch (astPeek(build))
    {
    case LexerToken::Equal:
        op = "==";
        break;
    case LexerToken::NotEqual:
        op = "!=";
        break;
    case LexerToken::LessThan:
        op = "<";
        break;
    case LexerToken::LessOrEqual:
        op = "<=";
        break;
    case LexerToken::GreaterThan:
        op = ">";
        break;
    case LexerToken::GreaterOrEqual:
        op = ">=";
        break;
    default:
        return true;
    }
    ++build.pos;
    std::unique_ptr<ast::Expr> rhs;
    if (!parseAstAdd(build, rhs))
    {
        return false;
    }
    expr = makeBinary(op, std::move(expr), std::move(rhs));
    return true;
}

bool parseAstAnd(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (!parseAstCmp(build, expr))
    {
        return false;
    }
    while (astKeywordIs(build, "and"))
    {
        ++build.pos;
        std::unique_ptr<ast::Expr> rhs;
        if (!parseAstCmp(build, rhs))
        {
            return false;
        }
        expr = makeBinary("and", std::move(expr), std::move(rhs));
    }
    return true;
}

bool parseAstXor(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (!parseAstAnd(build, expr))
    {
        return false;
    }
    while (astKeywordIs(build, "xor"))
    {
        ++build.pos;
        std::unique_ptr<ast::Expr> rhs;
        if (!parseAstAnd(build, rhs))
        {
            return false;
        }
        expr = makeBinary("xor", std::move(expr), std::move(rhs));
    }
    return true;
}

bool parseAstOr(AstBuild &build, std::unique_ptr<ast::Expr> &expr)
{
    if (!parseAstXor(build, expr))
    {
        return false;
    }
    while (astKeywordIs(build, "or"))
    {
        ++build.pos;
        std::unique_ptr<ast::Expr> rhs;
        if (!parseAstXor(build, rhs))
        {
            return false;
        }
        expr = makeBinary("or", std::move(expr), std::move(rhs));
    }
    return true;
}

bool parseAstExpr(AstBuild &build, std::unique_ptr<ast::Expr> &expr) { return parseAstOr(build, expr); }

bool parseAstBlock(AstBuild &build, std::vector<ast::Stmt> &body);
bool parseAstStmt(AstBuild &build, ast::Stmt &stmt);

bool parseAstIf(AstBuild &build, ast::Stmt &stmt)
{
    const SrcPos start = astTok(build).pos;
    ++build.pos; // consume 'if'
    if (!astExpect(build, LexerToken::OpenParenthesis, "'('"))
    {
        return false;
    }
    std::unique_ptr<ast::Expr> cond;
    if (!parseAstExpr(build, cond))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::CloseParenthesis, "')'"))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::OpenBrace, "'{'"))
    {
        return false;
    }
    ast::Stmt::If node;
    node.cond = std::move(cond);
    if (!parseAstBlock(build, node.thenBranch))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::CloseBrace, "'}'"))
    {
        return false;
    }
    if (astKeywordIs(build, "else"))
    {
        ++build.pos;
        node.hasElse = true;
        if (!astExpect(build, LexerToken::OpenBrace, "'{'"))
        {
            return false;
        }
        if (!parseAstBlock(build, node.elseBranch))
        {
            return false;
        }
        if (!astExpect(build, LexerToken::CloseBrace, "'}'"))
        {
            return false;
        }
    }
    stmt.pos = start;
    stmt.end = (*build.toks)[build.pos - 1].pos;
    stmt.node = std::move(node);
    return true;
}

bool parseAstWhile(AstBuild &build, ast::Stmt &stmt)
{
    const SrcPos start = astTok(build).pos;
    ++build.pos; // consume 'while'
    if (!astExpect(build, LexerToken::OpenParenthesis, "'('"))
    {
        return false;
    }
    std::unique_ptr<ast::Expr> cond;
    if (!parseAstExpr(build, cond))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::CloseParenthesis, "')'"))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::OpenBrace, "'{'"))
    {
        return false;
    }
    ast::Stmt::While node;
    node.cond = std::move(cond);
    if (!parseAstBlock(build, node.body))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::CloseBrace, "'}'"))
    {
        return false;
    }
    stmt.pos = start;
    stmt.end = (*build.toks)[build.pos - 1].pos;
    stmt.node = std::move(node);
    return true;
}

bool parseAstReturn(AstBuild &build, ast::Stmt &stmt)
{
    const SrcPos start = astTok(build).pos;
    ++build.pos; // consume 'return'
    ast::Stmt::Return node;
    if (astPeek(build) != LexerToken::Semicolon)
    {
        if (!parseAstExpr(build, node.value))
        {
            return false;
        }
    }
    if (!astExpect(build, LexerToken::Semicolon, "';'"))
    {
        return false;
    }
    stmt.pos = start;
    stmt.end = (*build.toks)[build.pos - 1].pos;
    stmt.node = std::move(node);
    return true;
}

bool parseAstStmt(AstBuild &build, ast::Stmt &stmt)
{
    if (astKeywordIs(build, "if"))
    {
        return parseAstIf(build, stmt);
    }
    if (astKeywordIs(build, "while"))
    {
        return parseAstWhile(build, stmt);
    }
    if (astKeywordIs(build, "return"))
    {
        return parseAstReturn(build, stmt);
    }
    if (astKeywordIs(build, "var"))
    {
        const SrcPos start = astTok(build).pos;
        ++build.pos;
        if (astPeek(build) != LexerToken::Identifier)
        {
            astError(build, "Expected a variable name.");
            return false;
        }
        ast::Stmt::VarDecl node;
        node.name = astTok(build).text;
        node.namePos = astTok(build).pos;
        ++build.pos;
        if (astPeek(build) == LexerToken::Assign)
        {
            ++build.pos;
            if (!parseAstExpr(build, node.init))
            {
                return false;
            }
        }
        if (!astExpect(build, LexerToken::Semicolon, "';'"))
        {
            return false;
        }
        stmt.pos = start;
        stmt.end = (*build.toks)[build.pos - 1].pos;
        stmt.node = std::move(node);
        return true;
    }
    if (astPeek(build) == LexerToken::Identifier)
    {
        // Assign looks like Ident '=' Expr ';' (Assign, not Equal).
        if (astPeek(build, 1) == LexerToken::Assign)
        {
            ast::Stmt::Assign node;
            node.name = astTok(build).text;
            node.namePos = astTok(build).pos;
            stmt.pos = astTok(build).pos;
            build.pos += 2;
            if (!parseAstExpr(build, node.value))
            {
                return false;
            }
            if (!astExpect(build, LexerToken::Semicolon, "';'"))
            {
                return false;
            }
            stmt.end = (*build.toks)[build.pos - 1].pos;
            stmt.node = std::move(node);
            return true;
        }
        const SrcPos start = astTok(build).pos;
        std::unique_ptr<ast::Expr> expr;
        if (!parseAstExpr(build, expr))
        {
            return false;
        }
        if (!astExpect(build, LexerToken::Semicolon, "';'"))
        {
            return false;
        }
        ast::Stmt::ExprStmt node;
        node.expr = std::move(expr);
        stmt.pos = start;
        stmt.end = (*build.toks)[build.pos - 1].pos;
        stmt.node = std::move(node);
        return true;
    }
    astError(build, "Expected a statement.");
    return false;
}

// Parses statements up to (but not consuming) the closing brace.
bool parseAstBlock(AstBuild &build, std::vector<ast::Stmt> &body)
{
    while (astPeek(build) != LexerToken::CloseBrace)
    {
        if (astAtEnd(build))
        {
            astError(build, "Expected '}'.");
            return false;
        }
        ast::Stmt stmt;
        if (!parseAstStmt(build, stmt))
        {
            return false;
        }
        body.push_back(std::move(stmt));
    }
    return true;
}

// Walks one function body collecting valued/bare returns from the Return
// nodes, including nested if/while branches, remembering the first
// position of each form so a mix points at the later return.
void classifyAstBody(const std::vector<ast::Stmt> &body, bool &valued, bool &bare, SrcPos &valuedPos, SrcPos &barePos)
{
    for (const auto &stmt : body)
    {
        if (const auto *ret = std::get_if<ast::Stmt::Return>(&stmt.node))
        {
            if (ret->value)
            {
                if (!valued)
                {
                    valuedPos = stmt.pos;
                }
                valued = true;
            }
            else
            {
                if (!bare)
                {
                    barePos = stmt.pos;
                }
                bare = true;
            }
        }
        else if (const auto *branch = std::get_if<ast::Stmt::If>(&stmt.node))
        {
            classifyAstBody(branch->thenBranch, valued, bare, valuedPos, barePos);
            classifyAstBody(branch->elseBranch, valued, bare, valuedPos, barePos);
        }
        else if (const auto *loop = std::get_if<ast::Stmt::While>(&stmt.node))
        {
            classifyAstBody(loop->body, valued, bare, valuedPos, barePos);
        }
    }
}

bool laterPos(SrcPos a, SrcPos b)
{
    if (a.line != b.line)
    {
        return a.line > b.line;
    }
    return a.col > b.col;
}

bool parseAstExternDecl(AstBuild &build, ast::Extern &ext, bool haveFunc, const std::vector<ast::Extern> &externs,
                        const std::vector<ast::Func> &funcs)
{
    const SrcPos start = astTok(build).pos;
    ++build.pos; // consume 'extern'
    if (haveFunc)
    {
        reportError(build.logger, build.sourceName, start, "Extern declarations must precede the function.",
                    "move extern declarations above func");
        return false;
    }
    if (astPeek(build) != LexerToken::Identifier)
    {
        astError(build, "Expected an extern name.");
        return false;
    }
    ext.name = astTok(build).text;
    const SrcPos namePos = astTok(build).pos;
    ++build.pos;
    for (const auto &seen : externs)
    {
        if (seen.name == ext.name)
        {
            reportError(build.logger, build.sourceName, namePos, "Duplicate extern name.", "use a different name");
            return false;
        }
    }
    for (const auto &seen : funcs)
    {
        if (seen.name == ext.name)
        {
            reportError(build.logger, build.sourceName, namePos, "Duplicate extern name.", "use a different name");
            return false;
        }
    }
    if (!astExpect(build, LexerToken::OpenParenthesis, "'('"))
    {
        return false;
    }
    if (astPeek(build) == LexerToken::Identifier)
    {
        ext.params.push_back(astTok(build).text);
        ++build.pos;
        while (astPeek(build) == LexerToken::Comma)
        {
            ++build.pos;
            if (astPeek(build) != LexerToken::Identifier)
            {
                astError(build, "Expected a parameter name.");
                return false;
            }
            ext.params.push_back(astTok(build).text);
            ++build.pos;
        }
    }
    if (!astExpect(build, LexerToken::CloseParenthesis, "')'"))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::Semicolon, "';'"))
    {
        return false;
    }
    ext.pos = start;
    ext.end = (*build.toks)[build.pos - 1].pos;
    return true;
}

bool parseAstFuncDecl(AstBuild &build, ast::Func &func, const std::vector<ast::Extern> &externs,
                      const std::vector<ast::Func> &funcs)
{
    const SrcPos start = astTok(build).pos;
    ++build.pos; // consume 'func'
    if (astPeek(build) != LexerToken::Identifier)
    {
        astError(build, "Expected a function name.");
        return false;
    }
    func.name = astTok(build).text;
    const SrcPos namePos = astTok(build).pos;
    ++build.pos;
    for (const auto &seen : externs)
    {
        if (seen.name == func.name)
        {
            reportError(build.logger, build.sourceName, namePos, "Duplicate function name.", "use a different name");
            return false;
        }
    }
    for (const auto &seen : funcs)
    {
        if (seen.name == func.name)
        {
            reportError(build.logger, build.sourceName, namePos, "Duplicate function name.", "use a different name");
            return false;
        }
    }
    if (astPeek(build) == LexerToken::OpenParenthesis)
    {
        ++build.pos;
        if (astPeek(build) == LexerToken::Identifier)
        {
            while (true)
            {
                const std::string param = astTok(build).text;
                const SrcPos paramPos = astTok(build).pos;
                for (const auto &seen : func.params)
                {
                    if (seen == param)
                    {
                        reportError(build.logger, build.sourceName, paramPos, "Duplicate parameter name.",
                                    "use a different name");
                        return false;
                    }
                }
                func.params.push_back(param);
                ++build.pos;
                if (astPeek(build) == LexerToken::Comma)
                {
                    ++build.pos;
                    if (astPeek(build) != LexerToken::Identifier)
                    {
                        astError(build, "Expected a parameter name.");
                        return false;
                    }
                    continue;
                }
                break;
            }
        }
        if (!astExpect(build, LexerToken::CloseParenthesis, "')'"))
        {
            return false;
        }
    }
    if (!astExpect(build, LexerToken::OpenBrace, "'{'"))
    {
        return false;
    }
    if (!parseAstBlock(build, func.body))
    {
        return false;
    }
    if (!astExpect(build, LexerToken::CloseBrace, "'}'"))
    {
        return false;
    }
    func.pos = start;
    func.end = (*build.toks)[build.pos - 1].pos;
    return true;
}

} // namespace

bool ast::parseProgram(const std::vector<Tok> &toks, kern::ILogger *logger, const std::string &sourceName,
                       ast::Program &program)
{
    program = ast::Program{};
    AstBuild build;
    build.toks = &toks;
    build.logger = logger;
    build.sourceName = sourceName;
    bool haveFunc = false;
    while (!astAtEnd(build))
    {
        if (astPeek(build) != LexerToken::Keyword)
        {
            astError(build, "Expected 'extern' or 'func'.");
            return false;
        }
        const std::string keyword = astTok(build).text;
        if (keyword == "extern")
        {
            ast::Extern ext;
            if (!parseAstExternDecl(build, ext, haveFunc, program.externs, program.funcs))
            {
                return false;
            }
            program.externs.push_back(std::move(ext));
        }
        else if (keyword == "func")
        {
            ast::Func func;
            if (!parseAstFuncDecl(build, func, program.externs, program.funcs))
            {
                return false;
            }
            program.funcs.push_back(std::move(func));
            haveFunc = true;
        }
        else
        {
            astError(build, "Expected 'extern' or 'func'.");
            return false;
        }
    }
    if (!haveFunc)
    {
        astError(build, "Expected 'func'.");
        return false;
    }
    for (auto &func : program.funcs)
    {
        SrcPos valuedPos{};
        SrcPos barePos{};
        classifyAstBody(func.body, func.hasValuedReturn, func.hasBareReturn, valuedPos, barePos);
        if (func.hasValuedReturn && func.hasBareReturn)
        {
            const SrcPos at = laterPos(valuedPos, barePos) ? valuedPos : barePos;
            reportError(logger, sourceName, at, "Mixed valued and void returns.",
                        "use one return form in each function");
            return false;
        }
    }
    return true;
}
