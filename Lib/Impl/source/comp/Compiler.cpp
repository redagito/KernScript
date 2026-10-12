#include "Compiler.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "common/Lexer.h"
#include "logging/Logger.h"

namespace {

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

bool atEnd(const Ctx &ctx) { return ctx.pos >= ctx.end; }

LexerToken peekType(const Ctx &ctx) {
  if (atEnd(ctx)) {
    return LexerToken::End;
  }
  return (*ctx.toks)[ctx.pos].type;
}

const std::string &peekText(const Ctx &ctx) {
  static const std::string empty;
  if (atEnd(ctx)) {
    return empty;
  }
  return (*ctx.toks)[ctx.pos].text;
}

bool expect(Ctx &ctx, LexerToken type, const std::string &what) {
  if (peekType(ctx) != type) {
    ctx.logger->error("Expected {}.", what);
    return false;
  }
  ++ctx.pos;
  return true;
}

bool markExternUse(Ctx &ctx, const std::string &name, bool valueUse) {
  const int want = valueUse ? 1 : 2;
  auto it = ctx.externUse->find(name);
  if (it == ctx.externUse->end()) {
    (*ctx.externUse)[name] = want;
    return true;
  }
  if (it->second != want) {
    ctx.logger->error("Inconsistent extern use.");
    return false;
  }
  return true;
}

// Emits one integer push for an unsigned magnitude (no sign). Rejects
// magnitudes above 4294967295. Uses pushi up to 2147483647, pushu above.
bool emitPushUint(Ctx &ctx, const std::string &digits, std::vector<std::string> &out) {
  unsigned long long value = 0;
  for (char c : digits) {
    if (c < '0' || c > '9') {
      ctx.logger->error("Invalid integer literal.");
      return false;
    }
    value = value * 10 + static_cast<unsigned long long>(c - '0');
    if (value > 4294967295ULL) {
      ctx.logger->error("Integer literal out of range.");
      return false;
    }
  }
  if (value <= 2147483647ULL) {
    out.push_back("pushi " + digits);
  } else {
    out.push_back("pushu " + digits);
  }
  return true;
}

bool parseExpr(Ctx &ctx, std::vector<std::string> &out);
bool parseStatement(Ctx &ctx, std::vector<std::string> &out);

// Parses '(' [Expr (',' Expr)*] ')' with the current token on '('.
// Each argument expression pushes exactly one net value; the emitted
// pushes run left to right so params fill in order.
bool parseCallArgs(Ctx &ctx, std::vector<std::string> &args, unsigned int &count) {
  if (!expect(ctx, LexerToken::OpenParenthesis, "'('")) {
    return false;
  }
  args.clear();
  count = 0;
  if (peekType(ctx) != LexerToken::CloseParenthesis) {
    while (true) {
      std::vector<std::string> one;
      if (!parseExpr(ctx, one)) {
        return false;
      }
      args.insert(args.end(), one.begin(), one.end());
      ++count;
      if (peekType(ctx) == LexerToken::Comma) {
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
bool lowerValueCall(Ctx &ctx, const std::string &name, std::vector<std::string> &out) {
  std::vector<std::string> args;
  unsigned int count = 0;
  if (!parseCallArgs(ctx, args, count)) {
    return false;
  }
  const auto ext = ctx.externs->find(name);
  if (ext != ctx.externs->end()) {
    if (count != ext->second) {
      ctx.logger->error("Wrong argument count.");
      return false;
    }
    if (!markExternUse(ctx, name, true)) {
      return false;
    }
    out.insert(out.end(), args.begin(), args.end());
    out.push_back("calle " + name);
    return true;
  }
  const auto fn = ctx.funcs->find(name);
  if (fn != ctx.funcs->end()) {
    if (count != fn->second.params.size()) {
      ctx.logger->error("Wrong argument count.");
      return false;
    }
    if (!fn->second.hasValuedReturn) {
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
bool parseCallStmt(Ctx &ctx, const std::string &name, std::vector<std::string> &out) {
  std::vector<std::string> args;
  unsigned int count = 0;
  if (!parseCallArgs(ctx, args, count)) {
    return false;
  }
  if (!expect(ctx, LexerToken::Semicolon, "';'")) {
    return false;
  }
  const auto ext = ctx.externs->find(name);
  if (ext != ctx.externs->end()) {
    if (count != ext->second) {
      ctx.logger->error("Wrong argument count.");
      return false;
    }
    if (!markExternUse(ctx, name, false)) {
      return false;
    }
    out.insert(out.end(), args.begin(), args.end());
    out.push_back("calle " + name);
    return true;
  }
  const auto fn = ctx.funcs->find(name);
  if (fn != ctx.funcs->end()) {
    if (count != fn->second.params.size()) {
      ctx.logger->error("Wrong argument count.");
      return false;
    }
    out.insert(out.end(), args.begin(), args.end());
    out.push_back("call " + name);
    if (fn->second.hasValuedReturn) {
      out.push_back("pop");
    }
    return true;
  }
  ctx.logger->error("Call to undeclared function.");
  return false;
}

bool parsePrimary(Ctx &ctx, std::vector<std::string> &out) {
  if (atEnd(ctx)) {
    ctx.logger->error("Expected an expression.");
    return false;
  }
  const Tok tok = (*ctx.toks)[ctx.pos];
  if (tok.type == LexerToken::Integer) {
    ++ctx.pos;
    return emitPushUint(ctx, tok.text, out);
  }
  if (tok.type == LexerToken::Float) {
    ++ctx.pos;
    out.push_back("pushf " + tok.text);
    return true;
  }
  if (tok.type == LexerToken::String) {
    ++ctx.pos;
    out.push_back("pushs \"" + tok.text + "\"");
    return true;
  }
  if (tok.type == LexerToken::Identifier) {
    if (ctx.pos + 1 < ctx.end && (*ctx.toks)[ctx.pos + 1].type == LexerToken::OpenParenthesis) {
      const std::string name = tok.text;
      ++ctx.pos;
      return lowerValueCall(ctx, name, out);
    }
    if (ctx.vars.find(tok.text) == ctx.vars.end()) {
      ctx.logger->error("Use of undeclared variable.");
      return false;
    }
    ++ctx.pos;
    out.push_back("pushv " + tok.text);
    return true;
  }
  if (tok.type == LexerToken::OpenParenthesis) {
    ++ctx.pos;
    if (!parseExpr(ctx, out)) {
      return false;
    }
    return expect(ctx, LexerToken::CloseParenthesis, "')'");
  }
  ctx.logger->error("Expected an expression.");
  return false;
}

bool parseUnary(Ctx &ctx, std::vector<std::string> &out) {
  if (peekType(ctx) == LexerToken::Dash) {
    ++ctx.pos;
    std::vector<std::string> rhs;
    if (!parseUnary(ctx, rhs)) {
      return false;
    }
    // Unary minus as 0 - x: push 0, push x, sub = 0 - x.
    out.push_back("pushi 0");
    out.insert(out.end(), rhs.begin(), rhs.end());
    out.push_back("sub");
    return true;
  }
  return parsePrimary(ctx, out);
}

bool parseMul(Ctx &ctx, std::vector<std::string> &out) {
  if (!parseUnary(ctx, out)) {
    return false;
  }
  while (peekType(ctx) == LexerToken::Asterisk || peekType(ctx) == LexerToken::Slash) {
    const bool isMul = peekType(ctx) == LexerToken::Asterisk;
    ++ctx.pos;
    std::vector<std::string> rhs;
    if (!parseUnary(ctx, rhs)) {
      return false;
    }
    // Stack discipline: left already pushed, push right, then combine as
    // left OP right (sub/div are not commutative).
    out.insert(out.end(), rhs.begin(), rhs.end());
    out.push_back(isMul ? "mul" : "div");
  }
  return true;
}

bool parseAdd(Ctx &ctx, std::vector<std::string> &out) {
  if (!parseMul(ctx, out)) {
    return false;
  }
  while (peekType(ctx) == LexerToken::Plus || peekType(ctx) == LexerToken::Dash) {
    const bool isAdd = peekType(ctx) == LexerToken::Plus;
    ++ctx.pos;
    std::vector<std::string> rhs;
    if (!parseMul(ctx, rhs)) {
      return false;
    }
    out.insert(out.end(), rhs.begin(), rhs.end());
    out.push_back(isAdd ? "add" : "sub");
  }
  return true;
}

bool parseExpr(Ctx &ctx, std::vector<std::string> &out) { return parseAdd(ctx, out); }

// Parses a single-comparison condition: AddExpr (cmpOp AddExpr). Emits the
// left pushes followed by the right pushes and returns the jump mnemonic
// that reaches the else/end label when the condition does NOT hold, so the
// then-block runs on fallthrough. Anything beyond (bare truthiness, logic
// operators, chained comparisons) is a clean compile error.
bool parseCondition(Ctx &ctx, std::vector<std::string> &out, std::string &falseJump) {
  if (!parseAdd(ctx, out)) {
    return false;
  }
  std::string jump;
  switch (peekType(ctx)) {
  case LexerToken::Equal:
    jump = "jne";
    break;
  case LexerToken::NotEqual:
    jump = "je";
    break;
  case LexerToken::LessThan:
    jump = "jge";
    break;
  case LexerToken::LessOrEqual:
    jump = "jg";
    break;
  case LexerToken::GreaterThan:
    jump = "jle";
    break;
  case LexerToken::GreaterOrEqual:
    jump = "jl";
    break;
  default:
    ctx.logger->error("Expected a comparison.");
    return false;
  }
  ++ctx.pos;
  if (!parseAdd(ctx, out)) {
    return false;
  }
  falseJump = jump;
  return true;
}

// Parses statements up to (but not consuming) the closing brace.
bool parseStmtList(Ctx &ctx, std::vector<std::string> &out) {
  while (peekType(ctx) != LexerToken::CloseBrace) {
    if (atEnd(ctx)) {
      ctx.logger->error("Expected '}}'.");
      return false;
    }
    if (!parseStatement(ctx, out)) {
      return false;
    }
  }
  return true;
}

bool parseIf(Ctx &ctx, std::vector<std::string> &out) {
  ++ctx.pos; // consume 'if'
  if (!expect(ctx, LexerToken::OpenParenthesis, "'('")) {
    return false;
  }
  std::vector<std::string> cond;
  std::string falseJump;
  if (!parseCondition(ctx, cond, falseJump)) {
    return false;
  }
  if (!expect(ctx, LexerToken::CloseParenthesis, "')'")) {
    return false;
  }
  if (!expect(ctx, LexerToken::OpenBrace, "'{'")) {
    return false;
  }
  std::vector<std::string> thenBody;
  if (!parseStmtList(ctx, thenBody)) {
    return false;
  }
  if (!expect(ctx, LexerToken::CloseBrace, "'}'")) {
    return false;
  }
  bool hasElse = false;
  std::vector<std::string> elseBody;
  if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "else") {
    ++ctx.pos;
    hasElse = true;
    if (!expect(ctx, LexerToken::OpenBrace, "'{'")) {
      return false;
    }
    if (!parseStmtList(ctx, elseBody)) {
      return false;
    }
    if (!expect(ctx, LexerToken::CloseBrace, "'}'")) {
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
  if (hasElse) {
    out.push_back("jmp " + endLabel);
    out.push_back("label " + elseLabel);
    out.insert(out.end(), elseBody.begin(), elseBody.end());
    out.push_back("label " + endLabel);
  } else {
    out.push_back("label " + endLabel);
  }
  return true;
}

bool parseWhile(Ctx &ctx, std::vector<std::string> &out) {
  ++ctx.pos; // consume 'while'
  if (!expect(ctx, LexerToken::OpenParenthesis, "'('")) {
    return false;
  }
  std::vector<std::string> cond;
  std::string falseJump;
  if (!parseCondition(ctx, cond, falseJump)) {
    return false;
  }
  if (!expect(ctx, LexerToken::CloseParenthesis, "')'")) {
    return false;
  }
  if (!expect(ctx, LexerToken::OpenBrace, "'{'")) {
    return false;
  }
  std::vector<std::string> loopBody;
  if (!parseStmtList(ctx, loopBody)) {
    return false;
  }
  if (!expect(ctx, LexerToken::CloseBrace, "'}'")) {
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
bool parseReturn(Ctx &ctx, std::vector<std::string> &out) {
  ++ctx.pos; // consume 'return'
  if (peekType(ctx) == LexerToken::Semicolon) {
    ++ctx.pos;
    if (ctx.funcValued) {
      ctx.logger->error("Mixed valued and void returns.");
      return false;
    }
    out.push_back("ret");
    return true;
  }
  if (ctx.funcHasBare || !ctx.funcValued) {
    ctx.logger->error("Mixed valued and void returns.");
    return false;
  }
  std::vector<std::string> value;
  if (!parseExpr(ctx, value)) {
    return false;
  }
  if (!expect(ctx, LexerToken::Semicolon, "';'")) {
    return false;
  }
  out.insert(out.end(), value.begin(), value.end());
  out.push_back("popv " + ctx.retTemp);
  out.push_back("retv " + ctx.retTemp);
  return true;
}

bool parseStatement(Ctx &ctx, std::vector<std::string> &out) {
  if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "if") {
    return parseIf(ctx, out);
  }
  if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "while") {
    return parseWhile(ctx, out);
  }
  if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "return") {
    return parseReturn(ctx, out);
  }
  if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "var") {
    ++ctx.pos;
    if (peekType(ctx) != LexerToken::Identifier) {
      ctx.logger->error("Expected a variable name.");
      return false;
    }
    const std::string name = peekText(ctx);
    if (ctx.vars.find(name) != ctx.vars.end()) {
      ctx.logger->error("Duplicate variable name.");
      return false;
    }
    ++ctx.pos;
    ctx.vars[name] = static_cast<unsigned int>(ctx.paramOrder.size() + ctx.varOrder.size());
    ctx.varOrder.push_back(name);
    if (peekType(ctx) == LexerToken::Assign) {
      ++ctx.pos;
      std::vector<std::string> init;
      if (!parseExpr(ctx, init)) {
        return false;
      }
      out.insert(out.end(), init.begin(), init.end());
      out.push_back("popv " + name);
    }
    return expect(ctx, LexerToken::Semicolon, "';'");
  }
  if (peekType(ctx) == LexerToken::Identifier) {
    // Assign looks like Ident '=' Expr ';' (Assign, not Equal).
    if (ctx.pos + 1 < ctx.end && (*ctx.toks)[ctx.pos + 1].type == LexerToken::Assign) {
      const std::string name = peekText(ctx);
      if (ctx.vars.find(name) == ctx.vars.end()) {
        ctx.logger->error("Assignment to undeclared variable.");
        return false;
      }
      ctx.pos += 2;
      std::vector<std::string> rhs;
      if (!parseExpr(ctx, rhs)) {
        return false;
      }
      if (!expect(ctx, LexerToken::Semicolon, "';'")) {
        return false;
      }
      out.insert(out.end(), rhs.begin(), rhs.end());
      out.push_back("popv " + name);
      return true;
    }
    // A call used as a statement keeps extern statement-use; anything else
    // starting with an identifier is a general expression statement whose
    // value is discarded.
    if (ctx.pos + 1 < ctx.end && (*ctx.toks)[ctx.pos + 1].type == LexerToken::OpenParenthesis) {
      const std::string name = peekText(ctx);
      ++ctx.pos;
      return parseCallStmt(ctx, name, out);
    }
    std::vector<std::string> value;
    if (!parseExpr(ctx, value)) {
      return false;
    }
    if (!expect(ctx, LexerToken::Semicolon, "';'")) {
      return false;
    }
    out.insert(out.end(), value.begin(), value.end());
    out.push_back("pop");
    return true;
  }
  ctx.logger->error("Expected a statement.");
  return false;
}

} // namespace

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger()) {
  // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode) {
  // Slice 5: ExternDecl* + FuncDecl* where each FuncDecl is
  // 'func' Ident ['(' [Ident (',' Ident)*] ')'] '{' Stmt* '}' and Stmt is
  // VarDecl | Assign | If (optional Else) | While | Return | ExprStmt.
  // Calls lower as expressions (left-to-right args, 'call' leaves the
  // return value) or statements (valued script calls add 'pop'; extern
  // calls stay a bare 'calle'). A function is valued iff it contains
  // 'return expr;', void otherwise; mixing both is a compile error, as is
  // a void call used as a value, a wrong argument count, or either use of
  // an undeclared name. Each extern is tracked per value/statement use
  // and mixed use is rejected. Signatures and classification are
  // collected before any body lowers, so forward references resolve.
  // Conditions stay single CmpExpr comparisons over arithmetic operands
  // (calls allowed); logic operators and anything else return false.
  Lexer lexer(m_logger);
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
  std::vector<Tok> toks;
  while (true) {
    lexer.lex(sourceCode);
    const LexerToken type = lexer.getToken();
    if (type == LexerToken::End) {
      break;
    }
    if (type == LexerToken::Invalid) {
      m_logger->error("Invalid source.");
      return false;
    }
    if (type == LexerToken::NewLine || type == LexerToken::Comment) {
      continue;
    }
    Tok tok;
    tok.type = type;
    tok.text = lexer.getLexeme();
    toks.push_back(tok);
  }

  std::map<std::string, unsigned int> externs;
  std::vector<std::pair<std::string, unsigned int>> externOrder;
  std::vector<FuncInfo> funcOrder;
  std::map<std::string, FuncInfo> funcs;
  bool haveFunc = false;

  std::size_t pos = 0;
  const auto topAtEnd = [&]() { return pos >= toks.size(); };
  const auto topPeek = [&](std::size_t off) {
    if (pos + off >= toks.size()) {
      return LexerToken::End;
    }
    return toks[pos + off].type;
  };
  while (!topAtEnd()) {
    if (toks[pos].type != LexerToken::Keyword) {
      m_logger->error("Expected 'extern' or 'func'.");
      return false;
    }
    const std::string keyword = toks[pos].text;
    if (keyword == "extern") {
      ++pos;
      if (haveFunc) {
        m_logger->error("Extern declarations must precede the function.");
        return false;
      }
      if (topPeek(0) != LexerToken::Identifier) {
        m_logger->error("Expected an extern name.");
        return false;
      }
      const std::string name = toks[pos].text;
      ++pos;
      if (externs.find(name) != externs.end() || funcs.find(name) != funcs.end()) {
        m_logger->error("Duplicate extern name.");
        return false;
      }
      if (topPeek(0) != LexerToken::OpenParenthesis) {
        m_logger->error("Expected '('.");
        return false;
      }
      ++pos;
      unsigned int arity = 0;
      if (topPeek(0) == LexerToken::Identifier) {
        ++pos;
        arity = 1;
        while (topPeek(0) == LexerToken::Comma) {
          ++pos;
          if (topPeek(0) != LexerToken::Identifier) {
            m_logger->error("Expected a parameter name.");
            return false;
          }
          ++pos;
          ++arity;
        }
      }
      if (topPeek(0) != LexerToken::CloseParenthesis) {
        m_logger->error("Expected ')'.");
        return false;
      }
      ++pos;
      if (topPeek(0) != LexerToken::Semicolon) {
        m_logger->error("Expected ';'.");
        return false;
      }
      ++pos;
      externs[name] = arity;
      externOrder.push_back({name, arity});
    } else if (keyword == "func") {
      ++pos;
      if (topPeek(0) != LexerToken::Identifier) {
        m_logger->error("Expected a function name.");
        return false;
      }
      const std::string name = toks[pos].text;
      ++pos;
      if (externs.find(name) != externs.end() || funcs.find(name) != funcs.end()) {
        m_logger->error("Duplicate function name.");
        return false;
      }
      FuncInfo info;
      info.name = name;
      if (topPeek(0) == LexerToken::OpenParenthesis) {
        ++pos;
        if (topPeek(0) == LexerToken::Identifier) {
          while (true) {
            const std::string param = toks[pos].text;
            for (const auto &seen : info.params) {
              if (seen == param) {
                m_logger->error("Duplicate parameter name.");
                return false;
              }
            }
            info.params.push_back(param);
            ++pos;
            if (topPeek(0) == LexerToken::Comma) {
              ++pos;
              if (topPeek(0) != LexerToken::Identifier) {
                m_logger->error("Expected a parameter name.");
                return false;
              }
              continue;
            }
            break;
          }
        }
        if (topPeek(0) != LexerToken::CloseParenthesis) {
          m_logger->error("Expected ')'.");
          return false;
        }
        ++pos;
      }
      if (topPeek(0) != LexerToken::OpenBrace) {
        m_logger->error("Expected '{{'.");
        return false;
      }
      ++pos;
      info.bodyBegin = pos;
      unsigned int depth = 1;
      while (!topAtEnd() && depth > 0) {
        if (toks[pos].type == LexerToken::OpenBrace) {
          ++depth;
        } else if (toks[pos].type == LexerToken::CloseBrace) {
          --depth;
        }
        ++pos;
      }
      if (depth != 0) {
        m_logger->error("Expected '}}'.");
        return false;
      }
      info.bodyEnd = pos - 1;
      funcs[name] = info;
      funcOrder.push_back(info);
      haveFunc = true;
    } else {
      m_logger->error("Expected 'extern' or 'func'.");
      return false;
    }
  }

  if (!haveFunc) {
    m_logger->error("Expected 'func'.");
    return false;
  }

  // Classify valued/void per function before lowering any body so forward
  // references check against the final classification.
  for (auto &entry : funcOrder) {
    bool valued = false;
    bool bare = false;
    for (std::size_t i = entry.bodyBegin; i < entry.bodyEnd; ++i) {
      if (toks[i].type == LexerToken::Keyword && toks[i].text == "return") {
        if (i + 1 < entry.bodyEnd && toks[i + 1].type == LexerToken::Semicolon) {
          bare = true;
        } else {
          valued = true;
        }
      }
    }
    entry.hasValuedReturn = valued;
    entry.hasBareReturn = bare;
    funcs[entry.name].hasValuedReturn = valued;
    funcs[entry.name].hasBareReturn = bare;
    if (valued && bare) {
      m_logger->error("Mixed valued and void returns.");
      return false;
    }
  }

  std::map<std::string, int> externUse;
  struct Lowered {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> vars;
    std::vector<std::string> body;
  };
  std::vector<Lowered> lowered;
  for (const auto &entry : funcOrder) {
    Ctx ctx;
    ctx.toks = &toks;
    ctx.pos = entry.bodyBegin;
    ctx.end = entry.bodyEnd;
    ctx.externs = &externs;
    ctx.funcs = &funcs;
    ctx.externUse = &externUse;
    ctx.logger = m_logger;
    ctx.funcValued = entry.hasValuedReturn;
    ctx.funcHasBare = entry.hasBareReturn;
    for (const auto &param : entry.params) {
      ctx.vars[param] = static_cast<unsigned int>(ctx.paramOrder.size());
      ctx.paramOrder.push_back(param);
    }
    if (ctx.funcValued) {
      // Hidden spill slot for 'return expr;' lowering. The name must lex
      // as an identifier (leading letter) and must not collide with a
      // parameter or it would shadow an argument slot.
      std::string candidate = "retmp";
      unsigned int suffix = 0;
      while (ctx.vars.find(candidate) != ctx.vars.end()) {
        candidate = "retmp" + std::to_string(suffix++);
      }
      ctx.retTemp = candidate;
      ctx.vars[candidate] = static_cast<unsigned int>(ctx.paramOrder.size() + ctx.varOrder.size());
      ctx.varOrder.push_back(candidate);
    }
    while (ctx.pos < ctx.end) {
      if (!parseStatement(ctx, ctx.body)) {
        return false;
      }
    }
    Lowered out;
    out.name = entry.name;
    out.params = ctx.paramOrder;
    out.vars = ctx.varOrder;
    out.body = ctx.body;
    lowered.push_back(out);
  }

  for (const auto &entry : externOrder) {
    asmCode << "extern " << entry.first << " " << entry.second << "\n";
  }
  for (const auto &fn : lowered) {
    asmCode << "func " << fn.name << "\n{\n";
    for (const auto &param : fn.params) {
      asmCode << "param " << param << "\n";
    }
    for (const auto &name : fn.vars) {
      asmCode << "var " << name << "\n";
    }
    for (const auto &line : fn.body) {
      asmCode << line << "\n";
    }
    asmCode << "ret\n}\n";
  }
  return true;
}
