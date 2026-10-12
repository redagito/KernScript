#include "Compiler.h"

#include <string>

#include "common/Lexer.h"
#include "logging/Logger.h"

namespace {

LexerToken nextSignificant(Lexer &lexer, std::istream &stream) {
  do {
    lexer.lex(stream);
  } while (lexer.getToken() == LexerToken::NewLine || lexer.getToken() == LexerToken::Comment);
  return lexer.getToken();
}

} // namespace

Compiler::Compiler(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger()) {
  // Empty
}

bool Compiler::compile(std::istream &sourceCode, std::ostream &asmCode) {
  // Slice 0: one function with an empty block, 'func' Ident '{' '}'.
  Lexer lexer(m_logger);
  lexer.addKeyword("func");

  if (nextSignificant(lexer, sourceCode) != LexerToken::Keyword || lexer.getLexeme() != "func") {
    m_logger->error("Expected 'func'.");
    return false;
  }
  if (nextSignificant(lexer, sourceCode) != LexerToken::Identifier) {
    m_logger->error("Expected a function name.");
    return false;
  }
  const std::string name = lexer.getLexeme();
  if (nextSignificant(lexer, sourceCode) != LexerToken::OpenBrace) {
    m_logger->error("Expected '{{'.");
    return false;
  }
  if (nextSignificant(lexer, sourceCode) != LexerToken::CloseBrace) {
    m_logger->error("Expected '}}'.");
    return false;
  }
  if (nextSignificant(lexer, sourceCode) != LexerToken::End) {
    m_logger->error("Unexpected input after function.");
    return false;
  }

  asmCode << "func " << name << "\n{\nret\n}\n";
  return true;
}
