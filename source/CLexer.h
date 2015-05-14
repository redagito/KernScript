#pragma once

#include <istream>
#include <unordered_set>

#include "ELexerToken.h"

/**
* \brief Lexer class.
*/
class CLexer
{
public:
	/**
	* \brief Gets next token from stream.
	* \return Number of whitespaces skipped.
	* Writes token and internal lexeme.
	*/
	unsigned int getNextToken(std::istream& stream, ELexerToken& token);

	/**
	* \brief Returns the current lexeme, set by getNextToken().
	*/
	const std::string& getLexeme() const;

	void addKeyword(const std::string& keyword);
	void removeKeyword(const std::string& keyword);

protected:
	bool isKeyword(const std::string& text) const;

	void lexNumeric(std::istream& stream);
	void lexIdentifierOrKeyword(std::istream& stream);
	void lexString(std::istream& stream);
	void lexCommentOrDivideAssignOrSlash(std::istream& stream);

private:
	std::unordered_set<std::string> m_keywords;
	std::string m_currentLexeme;
	ELexerToken m_currentToken = ELexerToken::Invalid;
};