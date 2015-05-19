#pragma once

#include <istream>
#include <ostream>

namespace kern
{

/**
* \brief Assembler interface class.
*
* Parses assembly source code and writes byte code.
*/
class IAssembler
{
public:
	/**
	* \brief Virtual destructor for interface class.
	*/
	virtual ~IAssembler();
	
	/**
	* \brief Assembles source code into byte code.
	* \return True on success and false on error. 
	* Reads assembly source code from the input stream and creates
	* a byte code representation, which is serialized to the output stream.
	*/
	virtual bool assemble(::std::istream& asmCode, ::std::ostream& byteCode) = 0;
};

}