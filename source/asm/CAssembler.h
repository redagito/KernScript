#pragma once

#include <list>
#include <string>

#include "kern/IAssembler.h"

class CAssembler : public kern::IAssembler
{
public:
	bool assemble(std::istream& asmCode, std::ostream& byteCode);

private:
	
};