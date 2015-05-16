#include "CParser.h"

CParser::CParser()
{
	// Language specific keywords
	m_lexer.addKeyword("var"); // Variable declaration
	m_lexer.addKeyword("func"); // Function declaration
	m_lexer.addKeyword("extern"); // External function declaration

	m_lexer.addKeyword("true"); // Boolean true constant
	m_lexer.addKeyword("false"); // Boolean false constant

	m_lexer.addKeyword("if"); // If statement
	m_lexer.addKeyword("else"); // Else statement
	m_lexer.addKeyword("while"); // While loop
	m_lexer.addKeyword("return"); // Return statement
}

bool CParser::parse(std::istream& stream)
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

		case ELexerToken::Invalid:
			return false;
		default:
			break;
		}
	}
	while (m_lexer.getToken() != ELexerToken::End && m_lexer.getToken() != ELexerToken::Invalid);
	return true;
}