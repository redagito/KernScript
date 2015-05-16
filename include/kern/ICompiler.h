#pragma once

#include <istream>
#include <ostream>

namespace kern
{

class ICompiler
{
public:
	virtual bool compile(std::istream& sourceCode, std::ostream& asmCode) = 0;
};

}