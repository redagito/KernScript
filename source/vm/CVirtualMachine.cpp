#include "CVirtualMachine.h"

bool CVirtualMachine::load(std::istream& byteCode)
{
	return false;
}

void CVirtualMachine::addFunction(const std::string& functionName, kern::IExternFunction* function)
{
	m_externFunctions[functionName].reset(function);
}

void CVirtualMachine::removeFunction(const ::std::string& functionName)
{
	m_externFunctions.erase(functionName);
}

bool CVirtualMachine::popParameter(int& value)
{
	bool ret = m_runtimeStack.top().convert(value);
	m_runtimeStack.pop();
	return ret;
}

bool CVirtualMachine::popParameter(float& value)
{
	bool ret = m_runtimeStack.top().convert(value);
	m_runtimeStack.pop();
	return ret;
}

bool CVirtualMachine::popParameter(::std::string& value)
{
	bool ret = m_runtimeStack.top().convert(value);
	m_runtimeStack.pop();
	return ret;
}

void CVirtualMachine::setReturnValue()
{
	m_runtimeStack.push(CValue(0));
}

void CVirtualMachine::setReturnValue(int value)
{
	m_runtimeStack.push(CValue(value));
}

void CVirtualMachine::setReturnValue(float value)
{
	m_runtimeStack.push(CValue(value));
}

void CVirtualMachine::setReturnValue(const std::string& value)
{
	m_runtimeStack.push(CValue(value));
}