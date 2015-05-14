#include "CLexer.h"

#include <cctype>
#include <cstdio>
#include <cassert>

unsigned int CLexer::getNextToken(std::istream& stream, ELexerToken& token)
{
	// Reset lexeme and token
	m_currentLexeme.clear();
	m_currentToken = ELexerToken::Invalid;


	// Peek next char
	char next = stream.peek();

	// For counting whitespaces
	unsigned int blankSkipped = 0;
	// Skip blanks
	while (std::isblank(next))
	{
		// Skip blank
		stream.get();
		++blankSkipped;
		// Peek next
		next = stream.peek();
	}

	// Check for numeric values
	if (std::isdigit(next))
	{
		// The next lexeme is a numeric type, either int or float
		lexNumeric(stream);
	}
	else
	{
		switch (next)
		{
		case '/':
			// Might be comment "//" or divide and assign "/=" or slash
			// Multiline comments "/* ... */" currently not supported
			lexCommentOrDivideAssignOrSlash(stream);
			break;
		case '!':
		case '?':
		case '"':
			lexString(stream);
			break;
		default:
			// Error
		}
	}

	token = m_currentToken;
	return blankSkipped;
}

const std::string& CLexer::getLexeme() const
{
	return m_currentLexeme;
}

void CLexer::addKeyword(const std::string& keyword)
{
	m_keywords.insert(keyword);
}

void CLexer::removeKeyword(const std::string& keyword)
{
	m_keywords.erase(keyword);
}

bool CLexer::isKeyword(const std::string& text) const
{
	return m_keywords.find(text) != m_keywords.end();
}

void CLexer::lexNumeric(std::istream& stream)
{
	// Default is integer until proven otherwise
	m_currentToken = ELexerToken::Integer;

	// Loop through chars until non-digit is found
	while (std::isdigit(stream.peek()))
	{
		char c;
		stream.get(c);
		m_currentLexeme.push_back(c);
	}
	// 
	if (stream.peek() == '.')
	{
		
	}
}

void CLexer::lexIdentifierOrKeyword(std::istream& stream)
{
	// Already checked for leading alphabetic character
	// Valid characters in a, identifier are alphabetic, numeric and underscore
	while (std::isalnum(stream.peek()) || stream.peek() == '_')
	{
		// Build lexeme
		char c;
		stream.get(c);
		m_currentLexeme.push_back(c);
	}

	// Default to identifier
	m_currentToken = ELexerToken::Identifier;
	// Check if lexeme is keyword
	if (isKeyword(m_currentLexeme))
	{
		m_currentToken = ELexerToken::Keyword;
	}
}

void CLexer::lexString(std::istream& stream)
{
	// Throw away first quote
	assert(stream.peek() == '"');
	char c;
	stream.get(c);

	bool escaped = false;
	while (!(stream.peek() == '"' && !escaped))
	{
		stream.get(c);
		if (!escaped)
		{
			if (c == '\\')
			{
				escaped = true;
			}
		}
		else
		{
			escaped = false;
		}
		m_currentLexeme.push_back(c);
	}
	// Throw away last quote
	assert(stream.peek() == '"');
	stream.get(c);
	m_currentToken = ELexerToken::String;
}

void CLexer::lexCommentOrDivideAssignOrSlash(std::istream& stream)
{
	
}