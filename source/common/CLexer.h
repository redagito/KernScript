#pragma once

#include <istream>
#include <string>
#include <unordered_set>

#include "ELexerToken.h"

/**
* \brief Lexer class.
*/
class CLexer
{
public:
	/**
	* \brief Extracts next token/lexeme from stream.
	* \return Number of whitespaces skipped.
	* Writes token and internal lexeme.
	*/
	unsigned int lex(std::istream& stream);

	/**
	* \brief Returns the current lexeme, set by getNextToken().
	*/
	const std::string& getLexeme() const;
	ELexerToken getToken() const;

	void addKeyword(const std::string& keyword);
	void removeKeyword(const std::string& keyword);

	void ignoreNewLine(bool state);

protected:
	bool isKeyword(const std::string& text) const;

	void lexSingleCharLexeme(std::istream& stream, ELexerToken tokenType);
	void lexSingleOrDoubleCharLexeme(std::istream& stream, ELexerToken singleToken, char nextPossible, ELexerToken doubleToken);
	void lexSingleOrDoubleCharLexeme(std::istream& stream, ELexerToken singleToken, char nextPossible0, ELexerToken doubleToken0, char nextPossible1, ELexerToken doubleToken1);

	void lexNumeric(std::istream& stream);
	void lexIdentifierOrKeyword(std::istream& stream);
	void lexString(std::istream& stream);
	void lexComment(std::istream& stream);

private:
	std::unordered_set<std::string> m_keywords;
	std::string m_currentLexeme;
	ELexerToken m_currentToken = ELexerToken::Invalid;
	bool m_ignoreNewLine = false;
};