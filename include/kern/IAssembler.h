#pragma once

#include <istream>
#include <ostream>

namespace kern
{

class IAssembler
{
public:
	virtual bool assemble(std::istream& asmCode, std::ostream& byteCode) = 0;
};

}