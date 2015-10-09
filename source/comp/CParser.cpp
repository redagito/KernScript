#include <cassert>
#include <iostream>

#include "CParser.h"

CParser::CParser()
{
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

bool CParser::parse(std::istream &stream)
{
  m_currentLineIndex = 0;
  m_currentLineNumber = 1;

  do
  {
    // Get lexeme
    m_currentLineIndex += m_lexer.lex(stream);

    // Process lexeme
    switch (m_lexer.getToken())
    {
    case ELexerToken::NewLine:
      ++m_currentLineNumber;
      m_currentLineIndex = 0;
      break;
    case ELexerToken::Keyword:
      if (m_lexer.getLexeme() == "func")
      {
        // Script function
        if (!parseFunction(stream))
        {
          return false;
        }
      }
      else if (m_lexer.getLexeme() == "extern")
      {
        // Extern function
        if (!parseExternFunction(stream))
        {
          return false;
        }
      }
    case ELexerToken::Invalid:
      return false;
    default:
      break;
    }
  } while (m_lexer.getToken() != ELexerToken::End &&
           m_lexer.getToken() != ELexerToken::Invalid);
  return true;
}

bool CParser::parseVariableDeclaration(std::istream &stream)
{
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

bool CParser::parseFunction(std::istream &stream)
{
  // Make sure the last token was 'func'
  assert(m_lexer.getToken() == ELexerToken::Keyword);
  assert(m_lexer.getLexeme() == "func");

  // Next should be function name identifier
  m_currentLineIndex += m_lexer.lex(stream);
  if (m_lexer.getToken() != ELexerToken::Identifier)
  {
    if (m_lexer.getToken() == ELexerToken::End)
    {
      std::cout << "Unexpected end of stream encountered while expecting a "
                   "function name identifier." << std::endl;
    }
    else
    {
      std::cout << "Invalid function name identifer '" << m_lexer.getLexeme()
                << "'." << std::endl;
    }
    return false;
  }

  // Store function name
  std::string functionName = m_lexer.getLexeme();

  // Next token should be '('
  m_currentLineIndex += m_lexer.lex(stream);
  if (m_lexer.getToken() != ELexerToken::OpenParenthesis)
  {
    return false;
  }

  // TODO Parameters

  // Next token should be ')'
  m_currentLineIndex += m_lexer.lex(stream);
  if (m_lexer.getToken() != ELexerToken::OpenParenthesis)
  {
    return false;
  }

  // After function header
  // 'func' Identifier '('  ')'
  // Expect function block
  parseBlock(stream);

  return false;
}

bool CParser::parseExternFunction(std::istream &stream) { return false; }

bool CParser::parseIf(std::istream &stream) { return false; }

bool CParser::parseElse(std::istream &stream) { return false; }

bool CParser::parseWhile(std::istream &stream) { return false; }

bool CParser::parseReturn(std::istream &stream) { return false; }

bool CParser::parseBlock(std::istream &stream) { return false; }