#pragma once

#include "kern/ICompiler.h"

class CCompiler
{
public:
	bool compile(std::istream& sourceCode, std::ostream& asmCode);
};