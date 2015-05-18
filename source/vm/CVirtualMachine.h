#pragma once

#include <stack>
#include <memory>
#include <unordered_map>

#include "kern/IVirtualMachine.h"
#include "kern/IExternFunction.h"

#include "CValue.h"
#include "common/Instructions.h"

class CVirtualMachine : public kern:: IVirtualMachine
{
public:
	bool load(std::istream& byteCode);

	bool runScript();

	void addFunction(const std::string& functionName, kern::IExternFunction* function);
	void removeFunction(const std::string& functionName);

	bool popParameter(int& value);
	bool popParameter(float& value);
	bool popParameter(std::string& value);

	void setReturnValue();
	void setReturnValue(int value);
	void setReturnValue(float value);
	void setReturnValue(const std::string& value);
	
protected:
	bool executeInstruction(InstructionId id);
	
private:
	std::unordered_map<std::string, std::unique_ptr<kern::IExternFunction>> m_externFunctions;
	std::stack<CValue> m_runtimeStack;
};