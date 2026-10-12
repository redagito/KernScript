#include "comp/Parser.h"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <string>
#include <utility>
#include <vector>

#include "common/Lexer.h"

bool tokenize(std::istream &sourceCode, kern::ILogger *logger, std::vector<Tok> &toks)
{
    Lexer lexer(logger);
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
            logger->error("Invalid source.");
            return false;
        }
        if (type == LexerToken::NewLine || type == LexerToken::Comment)
        {
            continue;
        }
        Tok tok;
        tok.type = type;
        tok.text = lexer.getLexeme();
        toks.push_back(tok);
    }
    return true;
}

bool topAtEnd(std::size_t pos, const std::vector<Tok> &toks) { return pos >= toks.size(); }

LexerToken topPeek(std::size_t pos, std::size_t off, const std::vector<Tok> &toks)
{
    if (pos + off >= toks.size())
    {
        return LexerToken::End;
    }
    return toks[pos + off].type;
}

bool parseProgram(const std::vector<Tok> &toks, kern::ILogger *logger, Program &program)
{
    // Slice 7: ExternDecl* + FuncDecl* where each FuncDecl is
    // 'func' Ident ['(' [Ident (',' Ident)*] ')'] '{' Stmt* '}'. Signatures
    // and classification are collected before any body lowers, so forward
    // references resolve.
    program = Program{};
    std::size_t pos = 0;
    bool haveFunc = false;
    while (!topAtEnd(pos, toks))
    {
        if (toks[pos].type != LexerToken::Keyword)
        {
            logger->error("Expected 'extern' or 'func'.");
            return false;
        }
        const std::string keyword = toks[pos].text;
        if (keyword == "extern")
        {
            ++pos;
            if (haveFunc)
            {
                logger->error("Extern declarations must precede the function.");
                return false;
            }
            if (topPeek(pos, 0, toks) != LexerToken::Identifier)
            {
                logger->error("Expected an extern name.");
                return false;
            }
            const std::string name = toks[pos].text;
            ++pos;
            if (program.externs.find(name) != program.externs.end() || program.funcs.find(name) != program.funcs.end())
            {
                logger->error("Duplicate extern name.");
                return false;
            }
            if (topPeek(pos, 0, toks) != LexerToken::OpenParenthesis)
            {
                logger->error("Expected '('.");
                return false;
            }
            ++pos;
            unsigned int arity = 0;
            if (topPeek(pos, 0, toks) == LexerToken::Identifier)
            {
                ++pos;
                arity = 1;
                while (topPeek(pos, 0, toks) == LexerToken::Comma)
                {
                    ++pos;
                    if (topPeek(pos, 0, toks) != LexerToken::Identifier)
                    {
                        logger->error("Expected a parameter name.");
                        return false;
                    }
                    ++pos;
                    ++arity;
                }
            }
            if (topPeek(pos, 0, toks) != LexerToken::CloseParenthesis)
            {
                logger->error("Expected ')'.");
                return false;
            }
            ++pos;
            if (topPeek(pos, 0, toks) != LexerToken::Semicolon)
            {
                logger->error("Expected ';'.");
                return false;
            }
            ++pos;
            program.externs[name] = arity;
            program.externOrder.push_back({name, arity});
        }
        else if (keyword == "func")
        {
            ++pos;
            if (topPeek(pos, 0, toks) != LexerToken::Identifier)
            {
                logger->error("Expected a function name.");
                return false;
            }
            const std::string name = toks[pos].text;
            ++pos;
            if (program.externs.find(name) != program.externs.end() || program.funcs.find(name) != program.funcs.end())
            {
                logger->error("Duplicate function name.");
                return false;
            }
            FuncInfo info;
            info.name = name;
            if (topPeek(pos, 0, toks) == LexerToken::OpenParenthesis)
            {
                ++pos;
                if (topPeek(pos, 0, toks) == LexerToken::Identifier)
                {
                    while (true)
                    {
                        const std::string param = toks[pos].text;
                        for (const auto &seen : info.params)
                        {
                            if (seen == param)
                            {
                                logger->error("Duplicate parameter name.");
                                return false;
                            }
                        }
                        info.params.push_back(param);
                        ++pos;
                        if (topPeek(pos, 0, toks) == LexerToken::Comma)
                        {
                            ++pos;
                            if (topPeek(pos, 0, toks) != LexerToken::Identifier)
                            {
                                logger->error("Expected a parameter name.");
                                return false;
                            }
                            continue;
                        }
                        break;
                    }
                }
                if (topPeek(pos, 0, toks) != LexerToken::CloseParenthesis)
                {
                    logger->error("Expected ')'.");
                    return false;
                }
                ++pos;
            }
            if (topPeek(pos, 0, toks) != LexerToken::OpenBrace)
            {
                logger->error("Expected '{{'.");
                return false;
            }
            ++pos;
            info.bodyBegin = pos;
            unsigned int depth = 1;
            while (!topAtEnd(pos, toks) && depth > 0)
            {
                if (toks[pos].type == LexerToken::OpenBrace)
                {
                    ++depth;
                }
                else if (toks[pos].type == LexerToken::CloseBrace)
                {
                    --depth;
                }
                ++pos;
            }
            if (depth != 0)
            {
                logger->error("Expected '}}'.");
                return false;
            }
            info.bodyEnd = pos - 1;
            program.funcs[name] = info;
            program.funcOrder.push_back(info);
            haveFunc = true;
        }
        else
        {
            logger->error("Expected 'extern' or 'func'.");
            return false;
        }
    }

    if (!haveFunc)
    {
        logger->error("Expected 'func'.");
        return false;
    }

    // Classify valued/void per function before lowering any body so forward
    // references check against the final classification.
    for (auto &entry : program.funcOrder)
    {
        bool valued = false;
        bool bare = false;
        for (std::size_t i = entry.bodyBegin; i < entry.bodyEnd; ++i)
        {
            if (toks[i].type == LexerToken::Keyword && toks[i].text == "return")
            {
                if (i + 1 < entry.bodyEnd && toks[i + 1].type == LexerToken::Semicolon)
                {
                    bare = true;
                }
                else
                {
                    valued = true;
                }
            }
        }
        entry.hasValuedReturn = valued;
        entry.hasBareReturn = bare;
        program.funcs[entry.name].hasValuedReturn = valued;
        program.funcs[entry.name].hasBareReturn = bare;
        if (valued && bare)
        {
            logger->error("Mixed valued and void returns.");
            return false;
        }
    }
    return true;
}

bool atEnd(const Ctx &ctx) { return ctx.pos >= ctx.end; }

LexerToken peekType(const Ctx &ctx)
{
    if (atEnd(ctx))
    {
        return LexerToken::End;
    }
    return (*ctx.toks)[ctx.pos].type;
}

const std::string &peekText(const Ctx &ctx)
{
    static const std::string empty;
    if (atEnd(ctx))
    {
        return empty;
    }
    return (*ctx.toks)[ctx.pos].text;
}

bool expect(Ctx &ctx, LexerToken type, const std::string &what)
{
    if (peekType(ctx) != type)
    {
        ctx.logger->error("Expected {}.", what);
        return false;
    }
    ++ctx.pos;
    return true;
}

bool markExternUse(Ctx &ctx, const std::string &name, bool valueUse)
{
    const int want = valueUse ? 1 : 2;
    auto it = ctx.externUse->find(name);
    if (it == ctx.externUse->end())
    {
        (*ctx.externUse)[name] = want;
        return true;
    }
    if (it->second != want)
    {
        ctx.logger->error("Inconsistent extern use.");
        return false;
    }
    return true;
}

// Emits one integer push for an unsigned magnitude (no sign). Rejects
// magnitudes above 4294967295. Uses pushi up to 2147483647, pushu above.
bool emitPushUint(Ctx &ctx, const std::string &digits, std::vector<std::string> &out)
{
    unsigned long long value = 0;
    for (char c : digits)
    {
        if (c < '0' || c > '9')
        {
            ctx.logger->error("Invalid integer literal.");
            return false;
        }
        value = value * 10 + static_cast<unsigned long long>(c - '0');
        if (value > 4294967295ULL)
        {
            ctx.logger->error("Integer literal out of range.");
            return false;
        }
    }
    if (value <= 2147483647ULL)
    {
        out.push_back("pushi " + digits);
    }
    else
    {
        out.push_back("pushu " + digits);
    }
    return true;
}

// Parses '(' [Expr (',' Expr)*] ')' with the current token on '('.
// Each argument expression pushes exactly one net value; the emitted
// pushes run left to right so params fill in order.
bool parseCallArgs(Ctx &ctx, std::vector<std::string> &args, unsigned int &count)
{
    if (!expect(ctx, LexerToken::OpenParenthesis, "'('"))
    {
        return false;
    }
    args.clear();
    count = 0;
    if (peekType(ctx) != LexerToken::CloseParenthesis)
    {
        while (true)
        {
            std::vector<std::string> one;
            bool oneIsString = false;
            if (!parseExpr(ctx, one, oneIsString))
            {
                return false;
            }
            args.insert(args.end(), one.begin(), one.end());
            ++count;
            if (peekType(ctx) == LexerToken::Comma)
            {
                ++ctx.pos;
                continue;
            }
            break;
        }
    }
    return expect(ctx, LexerToken::CloseParenthesis, "')'");
}

// Lowers a call used as a value. Extern calls mark value-use; script calls
// require a valued callee and leave the return value on the stack.
bool lowerValueCall(Ctx &ctx, const std::string &name, std::vector<std::string> &out)
{
    std::vector<std::string> args;
    unsigned int count = 0;
    if (!parseCallArgs(ctx, args, count))
    {
        return false;
    }
    const auto ext = ctx.externs->find(name);
    if (ext != ctx.externs->end())
    {
        if (count != ext->second)
        {
            ctx.logger->error("Wrong argument count.");
            return false;
        }
        if (!markExternUse(ctx, name, true))
        {
            return false;
        }
        out.insert(out.end(), args.begin(), args.end());
        out.push_back("calle " + name);
        return true;
    }
    const auto fn = ctx.funcs->find(name);
    if (fn != ctx.funcs->end())
    {
        if (count != fn->second.params.size())
        {
            ctx.logger->error("Wrong argument count.");
            return false;
        }
        if (!fn->second.hasValuedReturn)
        {
            ctx.logger->error("Void call used as a value.");
            return false;
        }
        out.insert(out.end(), args.begin(), args.end());
        out.push_back("call " + name);
        return true;
    }
    ctx.logger->error("Call to undeclared function.");
    return false;
}

// Lowers a call used as a statement (current token is '(' after the name).
// Valued script calls discard the return value with pop; extern calls mark
// statement-use and emit a bare calle.
bool parseCallStmt(Ctx &ctx, const std::string &name, std::vector<std::string> &out)
{
    std::vector<std::string> args;
    unsigned int count = 0;
    if (!parseCallArgs(ctx, args, count))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::Semicolon, "';'"))
    {
        return false;
    }
    const auto ext = ctx.externs->find(name);
    if (ext != ctx.externs->end())
    {
        if (count != ext->second)
        {
            ctx.logger->error("Wrong argument count.");
            return false;
        }
        if (!markExternUse(ctx, name, false))
        {
            return false;
        }
        out.insert(out.end(), args.begin(), args.end());
        out.push_back("calle " + name);
        return true;
    }
    const auto fn = ctx.funcs->find(name);
    if (fn != ctx.funcs->end())
    {
        if (count != fn->second.params.size())
        {
            ctx.logger->error("Wrong argument count.");
            return false;
        }
        out.insert(out.end(), args.begin(), args.end());
        out.push_back("call " + name);
        if (fn->second.hasValuedReturn)
        {
            out.push_back("pop");
        }
        return true;
    }
    ctx.logger->error("Call to undeclared function.");
    return false;
}

bool parsePrimary(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    isString = false;
    if (atEnd(ctx))
    {
        ctx.logger->error("Expected an expression.");
        return false;
    }
    const Tok tok = (*ctx.toks)[ctx.pos];
    if (tok.type == LexerToken::Integer)
    {
        ++ctx.pos;
        return emitPushUint(ctx, tok.text, out);
    }
    if (tok.type == LexerToken::Float)
    {
        ++ctx.pos;
        errno = 0;
        char *end = nullptr;
        const float value = std::strtof(tok.text.c_str(), &end);
        if (end == nullptr || *end != '\0' || !std::isfinite(value))
        {
            ctx.logger->error("Float literal out of range.");
            return false;
        }
        out.push_back("pushf " + tok.text);
        return true;
    }
    if (tok.type == LexerToken::String)
    {
        ++ctx.pos;
        out.push_back("pushs \"" + tok.text + "\"");
        isString = true;
        return true;
    }
    if (tok.type == LexerToken::Identifier)
    {
        if (ctx.pos + 1 < ctx.end && (*ctx.toks)[ctx.pos + 1].type == LexerToken::OpenParenthesis)
        {
            const std::string name = tok.text;
            ++ctx.pos;
            return lowerValueCall(ctx, name, out);
        }
        if (ctx.vars.find(tok.text) == ctx.vars.end())
        {
            ctx.logger->error("Use of undeclared variable.");
            return false;
        }
        ++ctx.pos;
        out.push_back("pushv " + tok.text);
        return true;
    }
    if (tok.type == LexerToken::OpenParenthesis)
    {
        ++ctx.pos;
        if (!parseExpr(ctx, out, isString))
        {
            return false;
        }
        return expect(ctx, LexerToken::CloseParenthesis, "')'");
    }
    ctx.logger->error("Expected an expression.");
    return false;
}

bool parseUnary(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    isString = false;
    if (peekType(ctx) == LexerToken::Dash)
    {
        ++ctx.pos;
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!parseUnary(ctx, rhs, rhsIsString))
        {
            return false;
        }
        if (rhsIsString)
        {
            ctx.logger->error("Arithmetic on strings.");
            return false;
        }
        // Unary minus as 0 - x: push 0, push x, sub = 0 - x.
        out.push_back("pushi 0");
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back("sub");
        return true;
    }
    if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "not")
    {
        ++ctx.pos;
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!parseUnary(ctx, rhs, rhsIsString))
        {
            return false;
        }
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back("not");
        return true;
    }
    return parsePrimary(ctx, out, isString);
}

bool parseMul(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    bool leftIsString = false;
    if (!parseUnary(ctx, out, leftIsString))
    {
        return false;
    }
    bool hadOp = false;
    while (peekType(ctx) == LexerToken::Asterisk || peekType(ctx) == LexerToken::Slash)
    {
        const bool isMul = peekType(ctx) == LexerToken::Asterisk;
        ++ctx.pos;
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!parseUnary(ctx, rhs, rhsIsString))
        {
            return false;
        }
        if (leftIsString || rhsIsString)
        {
            ctx.logger->error("Arithmetic on strings.");
            return false;
        }
        // Stack discipline: left already pushed, push right, then combine as
        // left OP right (sub/div are not commutative).
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back(isMul ? "mul" : "div");
        leftIsString = false;
        hadOp = true;
    }
    isString = hadOp ? false : leftIsString;
    return true;
}

bool parseAdd(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    bool leftIsString = false;
    if (!parseMul(ctx, out, leftIsString))
    {
        return false;
    }
    while (peekType(ctx) == LexerToken::Plus || peekType(ctx) == LexerToken::Dash)
    {
        const bool isAdd = peekType(ctx) == LexerToken::Plus;
        ++ctx.pos;
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!parseMul(ctx, rhs, rhsIsString))
        {
            return false;
        }
        if (leftIsString || rhsIsString)
        {
            ctx.logger->error("Arithmetic on strings.");
            return false;
        }
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back(isAdd ? "add" : "sub");
        leftIsString = false;
    }
    isString = leftIsString;
    return true;
}

bool parseExpr(Ctx &ctx, std::vector<std::string> &out, bool &isString) { return parseOr(ctx, out, isString); }

// Lowers one optional comparison to a 1/0 value. Plain arithmetic passes
// through; with a comparison operator the left/right pushes are followed by
// the true-jump over pushi 0 / pushi 1 so the stack holds exactly one
// numeric value for an enclosing logic operator or condition test.
bool parseCmp(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    std::vector<std::string> left;
    bool leftIsString = false;
    if (!parseAdd(ctx, left, leftIsString))
    {
        return false;
    }
    std::string trueJump;
    switch (peekType(ctx))
    {
    case LexerToken::Equal:
        trueJump = "je";
        break;
    case LexerToken::NotEqual:
        trueJump = "jne";
        break;
    case LexerToken::LessThan:
        trueJump = "jl";
        break;
    case LexerToken::LessOrEqual:
        trueJump = "jle";
        break;
    case LexerToken::GreaterThan:
        trueJump = "jg";
        break;
    case LexerToken::GreaterOrEqual:
        trueJump = "jge";
        break;
    default:
        out.insert(out.end(), left.begin(), left.end());
        isString = leftIsString;
        return true;
    }
    ++ctx.pos;
    std::vector<std::string> right;
    bool rightIsString = false;
    if (!parseAdd(ctx, right, rightIsString))
    {
        return false;
    }
    out.insert(out.end(), left.begin(), left.end());
    out.insert(out.end(), right.begin(), right.end());
    const unsigned int id = ctx.labelCounter++;
    const std::string trueLabel = "cmp_true_" + std::to_string(id);
    const std::string endLabel = "cmp_end_" + std::to_string(id);
    out.push_back(trueJump + " " + trueLabel);
    out.push_back("pushi 0");
    out.push_back("jmp " + endLabel);
    out.push_back("label " + trueLabel);
    out.push_back("pushi 1");
    out.push_back("label " + endLabel);
    isString = false;
    return true;
}

bool parseAnd(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    bool leftIsString = false;
    if (!parseCmp(ctx, out, leftIsString))
    {
        return false;
    }
    bool hadOp = false;
    while (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "and")
    {
        ++ctx.pos;
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!parseCmp(ctx, rhs, rhsIsString))
        {
            return false;
        }
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back("and");
        leftIsString = false;
        hadOp = true;
    }
    isString = hadOp ? false : leftIsString;
    return true;
}

bool parseXor(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    bool leftIsString = false;
    if (!parseAnd(ctx, out, leftIsString))
    {
        return false;
    }
    bool hadOp = false;
    while (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "xor")
    {
        ++ctx.pos;
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!parseAnd(ctx, rhs, rhsIsString))
        {
            return false;
        }
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back("xor");
        leftIsString = false;
        hadOp = true;
    }
    isString = hadOp ? false : leftIsString;
    return true;
}

bool parseOr(Ctx &ctx, std::vector<std::string> &out, bool &isString)
{
    bool leftIsString = false;
    if (!parseXor(ctx, out, leftIsString))
    {
        return false;
    }
    bool hadOp = false;
    while (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "or")
    {
        ++ctx.pos;
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!parseXor(ctx, rhs, rhsIsString))
        {
            return false;
        }
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back("or");
        leftIsString = false;
        hadOp = true;
    }
    isString = hadOp ? false : leftIsString;
    return true;
}

// Parses a full value condition for if/while. The expression always leaves
// exactly one numeric value (comparisons lower to 1/0 above); pushing 0 and
// je-ing to the else/end label jumps when the value is false, so the
// then-block runs on fallthrough.
bool parseCondition(Ctx &ctx, std::vector<std::string> &out, std::string &falseJump)
{
    bool valueIsString = false;
    if (!parseExpr(ctx, out, valueIsString))
    {
        return false;
    }
    out.push_back("pushi 0");
    falseJump = "je";
    return true;
}

// Parses statements up to (but not consuming) the closing brace.
bool parseStmtList(Ctx &ctx, std::vector<std::string> &out)
{
    while (peekType(ctx) != LexerToken::CloseBrace)
    {
        if (atEnd(ctx))
        {
            ctx.logger->error("Expected '}}'.");
            return false;
        }
        if (!parseStatement(ctx, out))
        {
            return false;
        }
    }
    return true;
}

bool parseIf(Ctx &ctx, std::vector<std::string> &out)
{
    ++ctx.pos; // consume 'if'
    if (!expect(ctx, LexerToken::OpenParenthesis, "'('"))
    {
        return false;
    }
    std::vector<std::string> cond;
    std::string falseJump;
    if (!parseCondition(ctx, cond, falseJump))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::CloseParenthesis, "')'"))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::OpenBrace, "'{'"))
    {
        return false;
    }
    std::vector<std::string> thenBody;
    if (!parseStmtList(ctx, thenBody))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::CloseBrace, "'}'"))
    {
        return false;
    }
    bool hasElse = false;
    std::vector<std::string> elseBody;
    if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "else")
    {
        ++ctx.pos;
        hasElse = true;
        if (!expect(ctx, LexerToken::OpenBrace, "'{'"))
        {
            return false;
        }
        if (!parseStmtList(ctx, elseBody))
        {
            return false;
        }
        if (!expect(ctx, LexerToken::CloseBrace, "'}'"))
        {
            return false;
        }
    }
    // Unique labels per if so nested and sequential ifs cannot collide.
    const unsigned int id = ctx.labelCounter++;
    const std::string elseLabel = "if_else_" + std::to_string(id);
    const std::string endLabel = "if_end_" + std::to_string(id);
    out.insert(out.end(), cond.begin(), cond.end());
    out.push_back(falseJump + " " + (hasElse ? elseLabel : endLabel));
    out.insert(out.end(), thenBody.begin(), thenBody.end());
    if (hasElse)
    {
        out.push_back("jmp " + endLabel);
        out.push_back("label " + elseLabel);
        out.insert(out.end(), elseBody.begin(), elseBody.end());
        out.push_back("label " + endLabel);
    }
    else
    {
        out.push_back("label " + endLabel);
    }
    return true;
}

bool parseWhile(Ctx &ctx, std::vector<std::string> &out)
{
    ++ctx.pos; // consume 'while'
    if (!expect(ctx, LexerToken::OpenParenthesis, "'('"))
    {
        return false;
    }
    std::vector<std::string> cond;
    std::string falseJump;
    if (!parseCondition(ctx, cond, falseJump))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::CloseParenthesis, "')'"))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::OpenBrace, "'{'"))
    {
        return false;
    }
    std::vector<std::string> loopBody;
    if (!parseStmtList(ctx, loopBody))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::CloseBrace, "'}'"))
    {
        return false;
    }
    // Unique labels per loop so sequential and nested whiles cannot collide.
    const unsigned int id = ctx.labelCounter++;
    const std::string startLabel = "while_start_" + std::to_string(id);
    const std::string endLabel = "while_end_" + std::to_string(id);
    out.push_back("label " + startLabel);
    out.insert(out.end(), cond.begin(), cond.end());
    out.push_back(falseJump + " " + endLabel);
    out.insert(out.end(), loopBody.begin(), loopBody.end());
    out.push_back("jmp " + startLabel);
    out.push_back("label " + endLabel);
    return true;
}

// Lowers 'return [Expr] ;'. A valued return evaluates the expression above
// the frame, spills it to the hidden return slot and returns it with retv;
// a bare return emits ret. Mixing both forms in one function is rejected.
bool parseReturn(Ctx &ctx, std::vector<std::string> &out)
{
    ++ctx.pos; // consume 'return'
    if (peekType(ctx) == LexerToken::Semicolon)
    {
        ++ctx.pos;
        if (ctx.funcValued)
        {
            ctx.logger->error("Mixed valued and void returns.");
            return false;
        }
        out.push_back("ret");
        return true;
    }
    if (ctx.funcHasBare || !ctx.funcValued)
    {
        ctx.logger->error("Mixed valued and void returns.");
        return false;
    }
    std::vector<std::string> value;
    bool valueIsString = false;
    if (!parseExpr(ctx, value, valueIsString))
    {
        return false;
    }
    if (!expect(ctx, LexerToken::Semicolon, "';'"))
    {
        return false;
    }
    out.insert(out.end(), value.begin(), value.end());
    out.push_back("popv " + ctx.retTemp);
    out.push_back("retv " + ctx.retTemp);
    return true;
}

bool parseStatement(Ctx &ctx, std::vector<std::string> &out)
{
    if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "if")
    {
        return parseIf(ctx, out);
    }
    if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "while")
    {
        return parseWhile(ctx, out);
    }
    if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "return")
    {
        return parseReturn(ctx, out);
    }
    if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "var")
    {
        ++ctx.pos;
        if (peekType(ctx) != LexerToken::Identifier)
        {
            ctx.logger->error("Expected a variable name.");
            return false;
        }
        const std::string name = peekText(ctx);
        if (ctx.vars.find(name) != ctx.vars.end())
        {
            ctx.logger->error("Duplicate variable name.");
            return false;
        }
        ++ctx.pos;
        ctx.vars[name] = static_cast<unsigned int>(ctx.paramOrder.size() + ctx.varOrder.size());
        ctx.varOrder.push_back(name);
        if (peekType(ctx) == LexerToken::Assign)
        {
            ++ctx.pos;
            std::vector<std::string> init;
            bool initIsString = false;
            if (!parseExpr(ctx, init, initIsString))
            {
                return false;
            }
            out.insert(out.end(), init.begin(), init.end());
            out.push_back("popv " + name);
        }
        return expect(ctx, LexerToken::Semicolon, "';'");
    }
    if (peekType(ctx) == LexerToken::Identifier)
    {
        // Assign looks like Ident '=' Expr ';' (Assign, not Equal).
        if (ctx.pos + 1 < ctx.end && (*ctx.toks)[ctx.pos + 1].type == LexerToken::Assign)
        {
            const std::string name = peekText(ctx);
            if (ctx.vars.find(name) == ctx.vars.end())
            {
                ctx.logger->error("Assignment to undeclared variable.");
                return false;
            }
            ctx.pos += 2;
            std::vector<std::string> rhs;
            bool rhsIsString = false;
            if (!parseExpr(ctx, rhs, rhsIsString))
            {
                return false;
            }
            if (!expect(ctx, LexerToken::Semicolon, "';'"))
            {
                return false;
            }
            out.insert(out.end(), rhs.begin(), rhs.end());
            out.push_back("popv " + name);
            return true;
        }
        // A call used as a statement keeps extern statement-use; anything else
        // starting with an identifier is a general expression statement whose
        // value is discarded.
        if (ctx.pos + 1 < ctx.end && (*ctx.toks)[ctx.pos + 1].type == LexerToken::OpenParenthesis)
        {
            const std::string name = peekText(ctx);
            ++ctx.pos;
            return parseCallStmt(ctx, name, out);
        }
        std::vector<std::string> value;
        bool valueIsString = false;
        if (!parseExpr(ctx, value, valueIsString))
        {
            return false;
        }
        if (!expect(ctx, LexerToken::Semicolon, "';'"))
        {
            return false;
        }
        out.insert(out.end(), value.begin(), value.end());
        out.push_back("pop");
        return true;
    }
    ctx.logger->error("Expected a statement.");
    return false;
}
