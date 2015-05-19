#pragma once

#include <istream>
#include <ostream>

namespace kern
{

/**
* \brief Script compiler interface class.
*
* Reads and parses script source code from the input stream and writes the
* resulting assembly source code to the output stream.
*/
class ICompiler
{
public:
	/**
	* \brief Virtual destructor for interface class.
	*/
	virtual ~ICompiler();
	
	/**
	* \brief Compiles script source code into assembly source code.
	* \return True on success and false on error.
	*/
	virtual bool compile(std::istream& sourceCode, std::ostream& asmCode) = 0;
};

}