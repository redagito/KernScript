#pragma once

namespace kern
{

class IVirtualMachine;

/**
* \brief Extern function interface class.
*
* Provides a function interface for calling native functions from scripts.
*/
class IExternFunction
{
public:
	virtual ~IExternFunction();
	
	/**
	* \brief Returns number of expected function arguments.
	*/
	virtual unsigned int getArgumentCount() const = 0;
	
	/**
	* \brief Call function from script.
	*/
	virtual bool call(IVirtualMachine& vm) = 0;
};

}