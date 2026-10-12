#include "Compiler.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "common/Lexer.h"
#include "logging/Logger.h"

namespace {

LexerToken nextSignificant(Lexer &lexer, std::istream &stream) {
  do {
    lexer.lex(stream);
  } while (lexer.getToken() == LexerToken::NewLine || lexer.getToken() == LexerToken::Comment);
  return lexer.getToken();
}

// Parses one integer literal ([Dash] Integer) with spec ranges: signed
// -2147483648..2147483647 emit pushi, 2147483648..4294967295 emit pushu,
// anything larger fails. Appends the push line to out on success.
bool parseIntLiteral(Lexer &lexer, std::istream &stream, kern::ILogger *logger, std::string &out) {
  bool negative = false;
  if (lexer.getToken() == LexerToken::Dash) {
    negative = true;
    if (nextSignificant(lexer, stream) != LexerToken::Integer) {
      logger->error("Expected an integer literal.");
      return false;
    }
  } else if (lexer.getToken() != LexerToken::Integer) {
    logger->error("Expected an integer literal.");
    return false;
  }
  const std::string digits = lexer.getLexeme();
  unsigned long long value = 0;
  for (char c : digits) {
    value = value * 10 + static_cast<unsigned long long>(c - '0');
    if (value > 4294967295ULL) {
      logger->error("Integer literal out of range.");
      return false;
    }
  }
  if (negative) {
    if (value > 2147483648ULL) {
      logger->error("Integer literal out of range.");
      return false;
    }
    if (value == 2147483648ULL) {
      out = "pushi -2147483648";
    } else {
      out = "pushi -" + digits;
    }
  } else if (value <= 2147483647ULL) {
    out = "pushi " + digits;
  } else {
    out = "pushu " + digits;
  }
  return true;
}

} // namespace

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger()) {
  // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode) {
  // Slice 1: ExternDecl* followed by one 'func' Ident '{' CallStmt* '}'.
  Lexer lexer(m_logger);
  lexer.addKeyword("func");
  lexer.addKeyword("extern");

  std::map<std::string, unsigned int> externs;
  std::vector<std::pair<std::string, unsigned int>> externOrder;
  std::string funcName;
  std::vector<std::string> body;
  bool haveFunc = false;

  LexerToken token = nextSignificant(lexer, sourceCode);
  while (token != LexerToken::End) {
    if (token != LexerToken::Keyword) {
      m_logger->error("Expected 'extern' or 'func'.");
      return false;
    }
    const std::string keyword = lexer.getLexeme();
    if (keyword == "extern") {
      if (haveFunc) {
        m_logger->error("Extern declarations must precede the function.");
        return false;
      }
      if (nextSignificant(lexer, sourceCode) != LexerToken::Identifier) {
        m_logger->error("Expected an extern name.");
        return false;
      }
      const std::string name = lexer.getLexeme();
      if (externs.find(name) != externs.end()) {
        m_logger->error("Duplicate extern name.");
        return false;
      }
      if (nextSignificant(lexer, sourceCode) != LexerToken::OpenParenthesis) {
        m_logger->error("Expected '('.");
        return false;
      }
      unsigned int arity = 0;
      token = nextSignificant(lexer, sourceCode);
      if (token == LexerToken::Identifier) {
        arity = 1;
        token = nextSignificant(lexer, sourceCode);
        while (token == LexerToken::Comma) {
          if (nextSignificant(lexer, sourceCode) != LexerToken::Identifier) {
            m_logger->error("Expected a parameter name.");
            return false;
          }
          ++arity;
          token = nextSignificant(lexer, sourceCode);
        }
      }
      if (token != LexerToken::CloseParenthesis) {
        m_logger->error("Expected ')'.");
        return false;
      }
      if (nextSignificant(lexer, sourceCode) != LexerToken::Semicolon) {
        m_logger->error("Expected ';'.");
        return false;
      }
      externs[name] = arity;
      externOrder.push_back({name, arity});
    } else if (keyword == "func") {
      if (haveFunc) {
        m_logger->error("Only one function is supported.");
        return false;
      }
      if (nextSignificant(lexer, sourceCode) != LexerToken::Identifier) {
        m_logger->error("Expected a function name.");
        return false;
      }
      funcName = lexer.getLexeme();
      if (nextSignificant(lexer, sourceCode) != LexerToken::OpenBrace) {
        m_logger->error("Expected '{{'.");
        return false;
      }
      token = nextSignificant(lexer, sourceCode);
      while (token != LexerToken::CloseBrace) {
        if (token != LexerToken::Identifier) {
          m_logger->error("Expected a call statement.");
          return false;
        }
        const std::string callee = lexer.getLexeme();
        const auto it = externs.find(callee);
        if (it == externs.end()) {
          m_logger->error("Call to undeclared extern.");
          return false;
        }
        if (nextSignificant(lexer, sourceCode) != LexerToken::OpenParenthesis) {
          m_logger->error("Expected '('.");
          return false;
        }
        std::vector<std::string> pushes;
        token = nextSignificant(lexer, sourceCode);
        if (token != LexerToken::CloseParenthesis) {
          while (true) {
            std::string push;
            if (!parseIntLiteral(lexer, sourceCode, m_logger, push)) {
              return false;
            }
            pushes.push_back(push);
            token = nextSignificant(lexer, sourceCode);
            if (token == LexerToken::Comma) {
              token = nextSignificant(lexer, sourceCode);
              continue;
            }
            break;
          }
          if (token != LexerToken::CloseParenthesis) {
            m_logger->error("Expected ')'.");
            return false;
          }
        }
        if (pushes.size() != it->second) {
          m_logger->error("Wrong argument count.");
          return false;
        }
        if (nextSignificant(lexer, sourceCode) != LexerToken::Semicolon) {
          m_logger->error("Expected ';'.");
          return false;
        }
        for (const auto &push : pushes) {
          body.push_back(push);
        }
        body.push_back("calle " + callee);
        token = nextSignificant(lexer, sourceCode);
      }
      haveFunc = true;
    } else {
      m_logger->error("Expected 'extern' or 'func'.");
      return false;
    }
    token = nextSignificant(lexer, sourceCode);
  }

  if (!haveFunc) {
    m_logger->error("Expected 'func'.");
    return false;
  }

  for (const auto &entry : externOrder) {
    asmCode << "extern " << entry.first << " " << entry.second << "\n";
  }
  asmCode << "func " << funcName << "\n{\n";
  for (const auto &line : body) {
    asmCode << line << "\n";
  }
  asmCode << "ret\n}\n";
  return true;
}
