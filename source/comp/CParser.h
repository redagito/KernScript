#pragma once

#include <istream>

#include "common/CLexer.h"

class CParser
{
public:
	CParser();

	bool parse(std::istream& sourceCode);

private:
	unsigned int m_currentLineNumber = 1;
	unsigned int m_currentLineIndex = 0;
	CLexer m_lexer;
};