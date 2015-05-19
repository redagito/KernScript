#include "CLexer.h"

#include <cctype>
#include <cstdio>
#include <cassert>
// TODO Debug only, remove later
#include <iostream>

unsigned int CLexer::lex(std::istream& stream)
{
	// Reset lexeme and token
	m_currentLexeme.clear();
	m_currentToken = ELexerToken::Invalid;

	// Peek next char
	char next = stream.peek();

	// For counting whitespaces
	unsigned int blankSkipped = 0;
	// Skip blanks
	while (next == ' ' || next == '\t' || m_ignoreNewLine && next == '\n')
	{
		// Skip blank
		stream.get();
		if (next != '\n')
		{
			++blankSkipped;
		}		
		// Peek next
		next = stream.peek();
	}

	// Check for numeric values
	if (std::isdigit(next))
	{
		// The next lexeme is a numeric type, either int or float
		lexNumeric(stream);
	}
	else if (std::isalpha(next))
	{
		// Either identifier or keyword
		lexIdentifierOrKeyword(stream);
	}
	else
	{
		switch (next)
		{
		case '.':
			lexSingleCharLexeme(stream, ELexerToken::Dot);
			break;
		case ':':
			// Either ':' or '::'
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Colon, ':', ELexerToken::DoubleColon);
			break;
		case ';':
			lexSingleCharLexeme(stream, ELexerToken::Semicolon);
			break;
		case ',':
			lexSingleCharLexeme(stream, ELexerToken::Comma);
			break;
		case '#':
			lexSingleCharLexeme(stream, ELexerToken::Pound);
			break;
		case '@':
			lexSingleCharLexeme(stream, ELexerToken::At);
			break;
		case '(':
			lexSingleCharLexeme(stream, ELexerToken::OpenParenthesis);
			break;
		case ')':
			lexSingleCharLexeme(stream, ELexerToken::CloseParenthesis);
			break;
		case '{':
			lexSingleCharLexeme(stream, ELexerToken::OpenBrace);
			break;
		case '}':
			lexSingleCharLexeme(stream, ELexerToken::CloseBrace);
			break;
		case '[':
			lexSingleCharLexeme(stream, ELexerToken::OpenBracket);
			break;
		case ']':
			lexSingleCharLexeme(stream, ELexerToken::CloseBracket);
			break;
		case '!':
			// Either just '!' or '!='
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::ExclamationMark, '=', ELexerToken::NotEqual);
			break;
		case '?':
			lexSingleCharLexeme(stream, ELexerToken::QuestionMark);
			break;
		case '&':
			// Either '&' or '&&'
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Ampersand, '&', ELexerToken::DoubleAmpersand);
			break;
		case '|':
			// Either '|' or '||'
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Pipe, '|', ELexerToken::DoublePipe);
			break;
		case '<':
			// Either '<' or '<='
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::LessThan, '=', ELexerToken::LessOrEqual);
			break;
		case '>':
			// Either '>' or '>='
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::GreaterThan, '=', ELexerToken::GreaterOrEqual);
			break;
		case '%':
			// Either '%' or '%='
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Percent, '=', ELexerToken::ModuloAssign);
			break;
		case '/':
			// Might be comment "//" or divide and assign "/=" or slash
			// Multiline comments "/* ... */" currently not supported
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Slash, '=', ELexerToken::DivideAssign, '/', ELexerToken::Comment);
			if (m_currentToken == ELexerToken::Comment)
			{
				lexComment(stream);
			}
			break;
		case '\\':
			lexSingleCharLexeme(stream, ELexerToken::BackSlash);
			break;
		case '=':
			// Either '=' or '=='
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Assign, '=', ELexerToken::Equal);
			break;
		case '*':
			// Either '*' or '*='
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Asterisk, '=', ELexerToken::MultiplyAssign);
			break;
		case '+':
			// Either '+' or '+=' or '++'
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Plus, '=', ELexerToken::AddAssign, '+', ELexerToken::Increment);
			break;
		case '-':
			// Either '-' or '-=' or '--'
			lexSingleOrDoubleCharLexeme(stream, ELexerToken::Dash, '=', ELexerToken::SubtractAssign, '-', ELexerToken::Decrement);
			break;
		case '"':
			lexString(stream);
			break;
		case '\n':
			lexSingleCharLexeme(stream, ELexerToken::NewLine);
			break;
		case EOF:
			m_currentToken = ELexerToken::End;
			break;
		default:
			// Error
			std::cout << "Lexer error: Invalid character '" << next << "' encountered." << std::endl;
			break;
		}
	}
	return blankSkipped;
}

const std::string& CLexer::getLexeme() const
{
	return m_currentLexeme;
}

ELexerToken CLexer::getToken() const
{
	return m_currentToken;
}

void CLexer::addKeyword(const std::string& keyword)
{
	m_keywords.insert(keyword);
}

void CLexer::removeKeyword(const std::string& keyword)
{
	m_keywords.erase(keyword);
}

void CLexer::ignoreNewLine(bool state)
{
	m_ignoreNewLine = state;
}

bool CLexer::isKeyword(const std::string& text) const
{
	return m_keywords.find(text) != m_keywords.end();
}

void CLexer::lexSingleCharLexeme(std::istream& stream, ELexerToken tokenType)
{
	// Retrieve next char
	char c;
	stream.get(c);
	// Store
	m_currentLexeme.push_back(c);
	// Set token
	m_currentToken = tokenType;
}

void CLexer::lexSingleOrDoubleCharLexeme(std::istream& stream, ELexerToken singleToken, char nextPossible, ELexerToken doubleToken)
{
	// Retrieve char and store
	char c;
	stream.get(c);
	m_currentLexeme.push_back(c);

	// Look ahead and check if lexeme is double char sequence
	if (stream.peek() == nextPossible)
	{
		// Add next char to lexeme
		stream.get(c);
		m_currentLexeme.push_back(c);
		// Set token to double token type
		m_currentToken = doubleToken;
	}
	else
	{
		// Set token to single token sequence
		m_currentToken = singleToken;
	}
}

void CLexer::lexSingleOrDoubleCharLexeme(std::istream& stream, ELexerToken singleToken, char nextPossible0, ELexerToken doubleToken0, char nextPossible1, ELexerToken doubleToken1)
{
	// Retrieve char and store
	char c;
	stream.get(c);
	m_currentLexeme.push_back(c);

	// Look ahead and check if lexeme is double char sequence
	if (stream.peek() == nextPossible0)
	{
		// Add next char to lexeme
		stream.get(c);
		m_currentLexeme.push_back(c);
		// Set token to double token type
		m_currentToken = doubleToken0;
	}
	else if (stream.peek() == nextPossible1)
	{
		// Add next char to lexeme
		stream.get(c);
		m_currentLexeme.push_back(c);
		// Set token to double token type
		m_currentToken = doubleToken1;
	}
	else
	{
		// Set token to single token sequence
		m_currentToken = singleToken;
	}
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

	// Check for dot
	if (stream.peek() != '.')
	{
		// Next token not a dot means end of integer, otherwise its a float
		return;
	}

	// After dot, at least one additional digit is required
	// Set token to invalid until at least one digit is read
	m_currentToken = ELexerToken::Invalid;

	char c;
	stream.get(c);
	m_currentLexeme.push_back(c);

	// Read remaining digits
	while (std::isdigit(stream.peek()))
	{
		// Valid float found
		m_currentToken = ELexerToken::Float;
		char c;
		stream.get(c);
		m_currentLexeme.push_back(c);
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

		// Check for line break (which is invalid in strings) or unexpected EOF stream signal
		if (stream.peek() == '\n' || stream.peek() == EOF)
		{
			m_currentToken = ELexerToken::Invalid;
			return;
		}
		m_currentLexeme.push_back(c);
	}
	// Throw away last quote
	assert(stream.peek() == '"');
	stream.get(c);
	m_currentToken = ELexerToken::String;
}

void CLexer::lexComment(std::istream& stream)
{
	// The '//' part has already been written, add remaining line
	// without newline character to lexeme 
	// Check for newline or end of stream
	while (!(stream.peek() == '\n' || stream.peek() == EOF))
	{
		char c;
		stream.get(c);
		m_currentLexeme.push_back(c);
	}
}