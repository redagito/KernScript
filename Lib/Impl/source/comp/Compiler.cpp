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

struct Ctx {
  std::vector<Tok> toks;
  std::size_t pos = 0;
  std::map<std::string, unsigned int> externs;
  std::vector<std::pair<std::string, unsigned int>> externOrder;
  std::map<std::string, unsigned int> vars;
  std::vector<std::string> varOrder;
  std::vector<std::string> body;
  std::string funcName;
  unsigned int labelCounter = 0;
  kern::ILogger *logger = nullptr;
};

bool atEnd(const Ctx &ctx) { return ctx.pos >= ctx.toks.size(); }

LexerToken peekType(const Ctx &ctx) {
  if (atEnd(ctx)) {
    return LexerToken::End;
  }
  return ctx.toks[ctx.pos].type;
}

const std::string &peekText(const Ctx &ctx) {
  static const std::string empty;
  if (atEnd(ctx)) {
    return empty;
  }
  return ctx.toks[ctx.pos].text;
}

bool expect(Ctx &ctx, LexerToken type, const std::string &what) {
  if (peekType(ctx) != type) {
    ctx.logger->error("Expected {}.", what);
    return false;
  }
  ++ctx.pos;
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

bool parsePrimary(Ctx &ctx, std::vector<std::string> &out) {
  if (atEnd(ctx)) {
    ctx.logger->error("Expected an expression.");
    return false;
  }
  const Tok tok = ctx.toks[ctx.pos];
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
    // A call used as a value is out of scope for this slice.
    if (ctx.pos + 1 < ctx.toks.size() && ctx.toks[ctx.pos + 1].type == LexerToken::OpenParenthesis) {
      ctx.logger->error("Calls as values are not supported.");
      return false;
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

// Parses an extern call statement, counting top-level arguments for the
// arity check. Each argument expression pushes exactly one net value.
bool parseCallStmtCounted(Ctx &ctx, std::vector<std::string> &out) {
  const std::string callee = peekText(ctx);
  const auto it = ctx.externs.find(callee);
  if (it == ctx.externs.end()) {
    ctx.logger->error("Call to undeclared extern.");
    return false;
  }
  ++ctx.pos;
  if (!expect(ctx, LexerToken::OpenParenthesis, "'('")) {
    return false;
  }
  std::vector<std::string> args;
  unsigned int count = 0;
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
  if (!expect(ctx, LexerToken::CloseParenthesis, "')'")) {
    return false;
  }
  if (count != it->second) {
    ctx.logger->error("Wrong argument count.");
    return false;
  }
  if (!expect(ctx, LexerToken::Semicolon, "';'")) {
    return false;
  }
  out.insert(out.end(), args.begin(), args.end());
  out.push_back("calle " + callee);
  return true;
}

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

bool parseStatement(Ctx &ctx, std::vector<std::string> &out) {
  if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "if") {
    return parseIf(ctx, out);
  }
  if (peekType(ctx) == LexerToken::Keyword && peekText(ctx) == "while") {
    return parseWhile(ctx, out);
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
    ctx.vars[name] = static_cast<unsigned int>(ctx.varOrder.size());
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
    if (ctx.pos + 1 < ctx.toks.size() && ctx.toks[ctx.pos + 1].type == LexerToken::Assign) {
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
    return parseCallStmtCounted(ctx, out);
  }
  ctx.logger->error("Expected a statement.");
  return false;
}

} // namespace

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger()) {
  // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode) {
  // Slice 4: ExternDecl* + one 'func' Ident ['()'] '{' Stmt* '}' where Stmt is
  // VarDecl | Assign | extern call (as in slice 2) plus If with an optional
  // Else and While. Conditions are single CmpExpr comparisons over
  // arithmetic operands; they lower to left/right pushes plus the inverse
  // conditional jump. If uses a unique per-if else/end label; while emits
  // start label, condition, inverse jump to end, body, jmp back to start
  // with unique per-loop labels (nesting-safe). Logic operators, params
  // and return remain compile errors.
  Lexer lexer(m_logger);
  lexer.addKeyword("func");
  lexer.addKeyword("extern");
  lexer.addKeyword("var");
  lexer.addKeyword("if");
  lexer.addKeyword("else");
  lexer.addKeyword("while");
  // Reserved logic operators: recognized so misuse fails with a clean
  // keyword error instead of parsing as an identifier. Slice 3 conditions
  // are single comparisons only.
  lexer.addKeyword("and");
  lexer.addKeyword("or");
  lexer.addKeyword("xor");
  lexer.addKeyword("not");

  // Buffer significant tokens for one-token lookahead.
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

  Ctx ctx;
  ctx.toks = toks;
  ctx.logger = m_logger;

  bool haveFunc = false;
  while (!atEnd(ctx)) {
    if (peekType(ctx) != LexerToken::Keyword) {
      m_logger->error("Expected 'extern' or 'func'.");
      return false;
    }
    const std::string keyword = peekText(ctx);
    if (keyword == "extern") {
      ++ctx.pos;
      if (haveFunc) {
        m_logger->error("Extern declarations must precede the function.");
        return false;
      }
      if (peekType(ctx) != LexerToken::Identifier) {
        m_logger->error("Expected an extern name.");
        return false;
      }
      const std::string name = peekText(ctx);
      ++ctx.pos;
      if (ctx.externs.find(name) != ctx.externs.end()) {
        m_logger->error("Duplicate extern name.");
        return false;
      }
      if (!expect(ctx, LexerToken::OpenParenthesis, "'('")) {
        return false;
      }
      unsigned int arity = 0;
      if (peekType(ctx) == LexerToken::Identifier) {
        ++ctx.pos;
        arity = 1;
        while (peekType(ctx) == LexerToken::Comma) {
          ++ctx.pos;
          if (peekType(ctx) != LexerToken::Identifier) {
            m_logger->error("Expected a parameter name.");
            return false;
          }
          ++ctx.pos;
          ++arity;
        }
      }
      if (!expect(ctx, LexerToken::CloseParenthesis, "')'")) {
        return false;
      }
      if (!expect(ctx, LexerToken::Semicolon, "';'")) {
        return false;
      }
      ctx.externs[name] = arity;
      ctx.externOrder.push_back({name, arity});
    } else if (keyword == "func") {
      ++ctx.pos;
      if (haveFunc) {
        m_logger->error("Only one function is supported.");
        return false;
      }
      if (peekType(ctx) != LexerToken::Identifier) {
        m_logger->error("Expected a function name.");
        return false;
      }
      ctx.funcName = peekText(ctx);
      ++ctx.pos;
      // Optional parameter list; only the empty form is supported yet.
      if (peekType(ctx) == LexerToken::OpenParenthesis) {
        ++ctx.pos;
        if (peekType(ctx) != LexerToken::CloseParenthesis) {
          m_logger->error("Function parameters are not supported.");
          return false;
        }
        ++ctx.pos;
      }
      if (!expect(ctx, LexerToken::OpenBrace, "'{'")) {
        return false;
      }
      while (peekType(ctx) != LexerToken::CloseBrace) {
        if (atEnd(ctx)) {
          m_logger->error("Expected '}}'.");
          return false;
        }
        if (!parseStatement(ctx, ctx.body)) {
          return false;
        }
      }
      ++ctx.pos;
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

  for (const auto &entry : ctx.externOrder) {
    asmCode << "extern " << entry.first << " " << entry.second << "\n";
  }
  asmCode << "func " << ctx.funcName << "\n{\n";
  for (const auto &name : ctx.varOrder) {
    asmCode << "var " << name << "\n";
  }
  for (const auto &line : ctx.body) {
    asmCode << line << "\n";
  }
  asmCode << "ret\n}\n";
  return true;
}
