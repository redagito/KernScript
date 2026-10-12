#include <cassert>

#include "comp/Parser.h"

#include "logging/Logger.h"

Parser::Parser(kern::ILogger *logger) : m_lexer(logger), m_logger(logger != nullptr ? logger : nullLogger()) {
  // Language specific keywords
  m_lexer.addKeyword("var");    // Variable declaration
  m_lexer.addKeyword("func");   // Function declaration
  m_lexer.addKeyword("extern"); // External function declaration

  m_lexer.addKeyword("true");  // Boolean true constant
  m_lexer.addKeyword("false"); // Boolean false constant

  m_lexer.addKeyword("if");     // If statement
  m_lexer.addKeyword("else");   // Else statement
  m_lexer.addKeyword("while");  // While loop
  m_lexer.addKeyword("return"); // Return statement
}

bool Parser::parse(std::istream &stream) {
  m_currentLineIndex = 0;
  m_currentLineNumber = 1;

  do {
    // Get lexeme
    m_currentLineIndex += m_lexer.lex(stream);

    // Process lexeme
    switch (m_lexer.getToken()) {
    case LexerToken::NewLine:
      ++m_currentLineNumber;
      m_currentLineIndex = 0;
      break;
    case LexerToken::Keyword:
      if (m_lexer.getLexeme() == "func") {
        // Script function
        if (!parseFunction(stream)) {
          return false;
        }
      } else if (m_lexer.getLexeme() == "extern") {
        // Extern function
        if (!parseExternFunction(stream)) {
          return false;
        }
      } else {
        m_logger->error("Invalid lexeme encountered: {}", m_lexer.getLexeme());
        return false;
      }
      break;
    case LexerToken::Invalid:
      return false;
    default:
      break;
    }
  } while (m_lexer.getToken() != LexerToken::End && m_lexer.getToken() != LexerToken::Invalid);
  return true;
}

bool Parser::parseVariableDeclaration(std::istream &stream) {
  // Variable declaration has the form
  // 'var' Identifier [ '[' Integer ']' ] [ '=' Expression ] ';'

  // A simple variable declaration would be
  // var a;
  // A variable declaration with assignment (declaration and definition) would
  // be
  // var a = 1;
  // var a = 1 + 2;
  // var a = "foo";
  // A variable array declaration would be
  // var a[10];
  return false;
}

bool Parser::parseFunction(std::istream &stream) {
  // Make sure the last token was 'func'
  assert(m_lexer.getToken() == LexerToken::Keyword);
  assert(m_lexer.getLexeme() == "func");

  // Next should be function name identifier
  m_currentLineIndex += m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::Identifier) {
    if (m_lexer.getToken() == LexerToken::End) {
      m_logger->error("Unexpected end of stream encountered while expecting a "
                      "function name identifier.");
    } else {
      m_logger->error("Invalid function name identifier '{}'.", m_lexer.getLexeme());
    }
    return false;
  }

  // Store function name
  std::string functionName = m_lexer.getLexeme();

  // Next token should be '('
  m_currentLineIndex += m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::OpenParenthesis) {
    return false;
  }

  // TODO Parameters

  // Next token should be ')'
  m_currentLineIndex += m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::CloseParenthesis) {
    return false;
  }

  // After function header
  // 'func' Identifier '('  ')'
  // Expect function block
  parseBlock(stream);

  return false;
}

bool Parser::parseExternFunction(std::istream &stream) { return false; }

bool Parser::parseIf(std::istream &stream) { return false; }

bool Parser::parseElse(std::istream &stream) { return false; }

bool Parser::parseWhile(std::istream &stream) { return false; }

bool Parser::parseReturn(std::istream &stream) { return false; }

bool Parser::parseBlock(std::istream &stream) { return false; }
