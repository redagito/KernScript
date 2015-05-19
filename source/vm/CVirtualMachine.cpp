#include "CVirtualMachine.h"

#include "common/Instructions.h"

bool CVirtualMachine::load(std::istream& byteCode)
{
	return false;
}

bool CVirtualMachine::runScript()
{
	return true;
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

bool CVirtualMachine::executeInstruction(InstructionId id)
{
	switch (id)
	{
	case instrNop:
		break;
	case instrPause:
		break;
	case instrExit:
		break;
	
	case instrPushi:
		break;
	case instrPop:
		break;

	case instrCall:
		break;
	case instrCalle:
		break;

	case instrAdd:
		break;
	case instrSub:
		break;
	case instrMul:
		break;
	case instrDiv:
		break;
	case instrInc:
		break;
	case instrDec:
		break;

	case instrAnd:
		break;
	case instrOr:
		break;
	case instrNot:
		break;
	case instrXor:
		break;

	case instrJmp:
		break;
	case instrJe:
		break;
	case instrJne:
		break;
	}

	return true;
}