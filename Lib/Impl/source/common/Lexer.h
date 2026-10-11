#pragma once

#include <istream>
#include <string>
#include <unordered_set>

#include <kern/ILogger.h>

#include "LexerToken.h"

/**
* \brief Lexer class.
*/
class Lexer
{
public:
  /**
  * \brief Constructs a lexer.
  * \param logger Receives diagnostics. A null pointer discards them.
  */
  explicit Lexer(kern::ILogger *logger);
  /**
  * \brief Extracts next token/lexeme from stream.
  * \return Number of whitespaces skipped.
  * Writes token and internal lexeme.
  */
  unsigned int lex(std::istream &stream);

  /**
  * \brief Returns the current lexeme, set by getNextToken().
  */
  const std::string &getLexeme() const;
  LexerToken getToken() const;

  void addKeyword(const std::string &keyword);
  void removeKeyword(const std::string &keyword);

  /**
  * \brief Ignore the next lex command and maintain current the state.
  */
  void ignoreNextLex();
  void ignoreNewLine(bool state);

protected:
  bool isKeyword(const std::string &text) const;

  void lexSingleCharLexeme(std::istream &stream, LexerToken tokenType);
  void lexSingleOrDoubleCharLexeme(std::istream &stream,
                                   LexerToken singleToken, char nextPossible,
                                   LexerToken doubleToken);
  void lexSingleOrDoubleCharLexeme(std::istream &stream,
                                   LexerToken singleToken, char nextPossible0,
                                   LexerToken doubleToken0, char nextPossible1,
                                   LexerToken doubleToken1);

  void lexNumeric(std::istream &stream);
  void lexIdentifierOrKeyword(std::istream &stream);
  void lexString(std::istream &stream);
  void lexComment(std::istream &stream);

private:
  std::unordered_set<std::string> m_keywords; /**< Registered keywords. */
  std::string m_currentLexeme;                /**< Currently parsed lexeme. */
  LexerToken m_currentToken = LexerToken::Invalid; /**< Current token. */
  bool m_ignoreNewLine = false;
  bool m_ignoreLex = false;
  unsigned int m_blankSkipped = 0;
  kern::ILogger *m_logger = nullptr; /**< Receives diagnostics. */
};