#pragma once

#include <stack>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

#include "kern/IVirtualMachine.h"
#include "kern/IExternFunction.h"

#include "CValue.h"
#include "common/SInstruction.h"
#include "common/SExternFunction.h"

class CVirtualMachine : public kern:: IVirtualMachine
{
public:
	bool load(std::istream& byteCode);

	void clearScripts();

	bool callFunction(const std::string& functionName);

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
	/**
	* \brief Byte code deserialization.
	*/
	bool deserializeStrings(std::istream& stream);
	bool deserializeExternFunctions(std::istream& stream);
	bool deserializeFunctions(std::istream& stream);

	bool getFunctionIndex(const std::string& functionName, uint32_t& index);

	bool execute(const SInstruction& instruction);
	
private:
	/**
	* \brief Assembled function.
	*/
	struct SFunction
	{
		std::string name; /**< Function name. */
		std::vector<SInstruction> instructions; /**< Assembled instructions. */
	};

	std::vector<std::string> m_strings; /**< String constants. */
	std::vector<SFunction> m_functions; /**< Assembled script functions. */
	std::vector<SExternFunction> m_externFunctions;

	std::unordered_map<std::string, std::unique_ptr<kern::IExternFunction>> m_externFunctionsMap; /**< Extern functions. */

	std::stack<CValue> m_runtimeStack; /**< Active runtuime stack. */
	uint32_t m_currentFunctionIndex; /**< Index of active function. */
	uint32_t m_currentInstructionIndex; /**< Index of active instruction. */
};