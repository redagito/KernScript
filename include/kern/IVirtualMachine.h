#pragma once

#include <string>
#include <istream>

namespace kern
{

class IExternFunction;

class IVirtualMachine
{
public:
	virtual ~IVirtualMachine();

	virtual bool load(std::istream& byteCode) = 0;
	virtual bool runScript() = 0;

	/**
	* \brief Extern function management.
	*/
	virtual void addFunction(const ::std::string& functionName, IExternFunction* function) = 0;
	virtual void removeFunction(const ::std::string& functionName) = 0;

	/**
	* \brief Interface for extern functions.
	*/
	virtual bool popParameter(int& value) = 0;
	virtual bool popParameter(float& value) = 0;
	virtual bool popParameter(::std::string& value) = 0;

	/**
	* \brief Sets empty return value for functions without return type.
	*/
	virtual void setReturnValue() = 0;
	virtual void setReturnValue(int value) = 0;
	virtual void setReturnValue(float value) = 0;
	virtual void setReturnValue(const std::string& value) = 0;
};

}