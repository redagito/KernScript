#include "comp/Compiler.h"

#include <map>
#include <string>
#include <vector>

#include "common/Lexer.h"
#include "comp/Parser.h"
#include "logging/Logger.h"

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger()) {
  // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode) {
  // Slice 7: ExternDecl* + FuncDecl* where each FuncDecl is
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
  // Conditions are full logic expressions (Or > Xor > And > Cmp, not at
  // Unary level); comparisons lower to 1/0 values and logic ops emit
  // and/or/xor/not. String and float literals lower to pushs/pushf; ==/!= compare strings
  // by value through je/jne. Arithmetic (+ - * / and unary minus) on a
  // statically known string operand is a compile error; string values
  // held in variables still fail at runtime.
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
