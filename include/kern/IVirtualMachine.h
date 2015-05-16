#pragma once

#include <string>

namespace kern
{

class IExternFunction;

class IVirtualMachine
{
public:
	virtual void addFunction(const ::std::string& functionName, IExternFunction* function) = 0;
	virtual void removeFunction(const ::std::string& functionName) = 0;

	/**
	* \brief Interface for extern functions.
	*/
	virtual bool popParameter(int& value) = 0;
	virtual bool popParameter(float& value) = 0;
	virtual bool popParameter(::std::string& value) = 0;

	virtual void setReturnValue(int value) = 0;
	virtual void setReturnValue(float value) = 0;
	virtual void setReturnValue(const std::string& value) = 0;
};

}