#include <iostream>

#include "CVirtualMachine.h"

#include "common/Instructions.h"


CVirtualMachine::SReturnData::SReturnData()
{

}

CVirtualMachine::SReturnData::SReturnData(uint32_t funcIndex, uint32_t instrIndex)
{
	functionIndex = funcIndex;
	instructionIndex = instrIndex;
}

CVirtualMachine::CVirtualMachine()
{
	// Reserve stack size
	m_runtimeStack.reserve(1024 * 256);
}

bool CVirtualMachine::load(std::istream& byteCode)
{
	// Load string constants
	if (!deserializeStrings(byteCode))
	{
		return false;
	}
	// Load extern functions
	if (!deserializeExternFunctions(byteCode))
	{
		return false;
	}
	// Load script functions
	if (!deserializeFunctions(byteCode))
	{
		return false;
	}
	return true;
}

void CVirtualMachine::clearScripts()
{
	m_currentFunctionIndex = 0;
	m_currentInstructionIndex = 0;
	// Clear script data
	m_strings.clear();
	m_externFunctions.clear();
	m_functions.clear();
}

bool CVirtualMachine::callFunction(const std::string& functionName)
{
	if (!getFunctionIndex(functionName, m_currentFunctionIndex))
	{
		return false;
	}
	m_currentInstructionIndex = 0;

	bool running = true;
	while (running)
	{
		running = execute();
	}
	return true;
}

void CVirtualMachine::addFunction(const std::string& functionName, kern::IExternFunction* function)
{
	m_externFunctionsMap[functionName].reset(function);
}

void CVirtualMachine::removeFunction(const ::std::string& functionName)
{
	m_externFunctionsMap.erase(functionName);
}

bool CVirtualMachine::popParameter(int& value)
{
	bool ret = m_runtimeStack.back().convert(value);
	m_runtimeStack.pop_back();
	return ret;
}

bool CVirtualMachine::popParameter(float& value)
{
	bool ret = m_runtimeStack.back().convert(value);
	m_runtimeStack.pop_back();
	return ret;
}

bool CVirtualMachine::popParameter(::std::string& value)
{
	bool ret = m_runtimeStack.back().convert(value);
	m_runtimeStack.pop_back();
	return ret;
}

void CVirtualMachine::setReturnValue()
{
	m_runtimeStack.push_back(CValue(0));
}

void CVirtualMachine::setReturnValue(int value)
{
	m_runtimeStack.push_back(CValue(value));
}

void CVirtualMachine::setReturnValue(float value)
{
	m_runtimeStack.push_back(CValue(value));
}

void CVirtualMachine::setReturnValue(const std::string& value)
{
	m_runtimeStack.push_back(CValue(value));
}

bool CVirtualMachine::deserializeStrings(std::istream& stream)
{
	uint32_t size = 0;
	stream.read((char*) &size, sizeof(size));
	m_strings.resize(size);	
	
	// Read strings
	for (auto& str : m_strings)
	{
		// Read length
		uint32_t length = 0;
		stream.read((char*) &length, sizeof(length));
		if (length == 0)
		{
			return false;
		}
		// Reserve space		
		str.reserve(length);
		
		for (uint32_t i = 0; i < length; ++i)
		{
			// TODO Slow?
			char c;
			stream.read(&c, 1);
			str.push_back(c);
		}
	}
	return stream.good();
}

bool CVirtualMachine::deserializeExternFunctions(std::istream& stream)
{
	// Read size
	uint32_t size = 0;
	stream.read((char*) &size, sizeof(size));
	m_externFunctions.resize(size);
	for (auto& externFunction : m_externFunctions)
	{
		if (!deserialize(externFunction, stream))
		{
			return false;
		}
	}
	return stream.good();
}

bool CVirtualMachine::deserializeFunctions(std::istream& stream)
{
	// Read size
	uint32_t size = 0;
	stream.read((char*) &size, sizeof(size));
	// Reserve space
	m_functions.resize(size);
	
	// Read function data
	for (auto& function : m_functions)
	{
		// Read function name
		uint32_t length = 0;
		stream.read((char*) &length, sizeof(length));
		function.name.reserve(length);
		for (uint32_t i = 0; i < length; ++i)
		{
			char c;
			stream.read(&c, 1);
			function.name.push_back(c);
		}

		// Read instructions
		// Instructions size
		stream.read((char*) &size, sizeof(size));
		function.instructions.resize(size);
		for (auto& instruction : function.instructions)
		{
			if (!deserialize(instruction, stream))
			{
				return false;
			}
		}
	}

	return stream.good();
}

bool CVirtualMachine::getFunctionIndex(const std::string& functionName, uint32_t& index)
{
	index = 0;
	for (const auto& function : m_functions)
	{
		if (function.name == functionName)
		{
			return true;
		}
		++index;
	}
	return false;
}

bool CVirtualMachine::execute()
{
	SInstruction instruction;
	// Check for out of bounds instruction index
	if (m_functions.at(m_currentFunctionIndex).instructions.size() <= m_currentInstructionIndex)
	{
		// Implicit return
		instruction.id = EInstruction::Ret;
	}
	else
	{
		// Retrieve current instruction
		instruction = m_functions.at(m_currentFunctionIndex).instructions.at(m_currentInstructionIndex);
	}

	switch (instruction.id)
	{
	case EInstruction::Nop:
		++m_currentInstructionIndex;
		break;
	case EInstruction::Break:
		// Not impelmented
		++m_currentInstructionIndex;
		break;
	case EInstruction::Exit:
		// Retunr false to signal end of script
		return false;
		break;
	
	case EInstruction::Pushi:
		// Arg 0 is integer value
		m_runtimeStack.push_back(CValue(instruction.args[0]));
		++m_currentInstructionIndex;
		break;
	case EInstruction::Pushf:
		// Arg 0 is float value
		m_runtimeStack.push_back(CValue(*((float*)&instruction.args[0])));
		++m_currentInstructionIndex;
		break;
	case EInstruction::Pop:
		// Remove top element from runtime stack
		m_runtimeStack.pop_back();
		++m_currentInstructionIndex;
		break;

	case EInstruction::Call:
		// Push next instruction and current function index for return call
		m_callStack.push(SReturnData(m_currentFunctionIndex, m_currentInstructionIndex + 1));
		// Arg 0 is function index
		// TODO Needs cast?
		m_currentFunctionIndex = instruction.args[0];
		// Reset instruction index
		m_currentInstructionIndex = 0;
		break;
	case EInstruction::Calle:
		{
			// Call external, arg 0 is extern function index
			const SExternFunction& externFunction = m_externFunctions.at(instruction.args[0]);
			
			// Check if function exists
			// TODO Check if arg count ok
			auto entry = m_externFunctionsMap.find(externFunction.name);
			if (entry == m_externFunctionsMap.end())
			{
				// Function does not exist
				std::cout << "The extern function '" << externFunction.name << "' does not exist." << std::endl;
				return false;
			}
			// Call if found
			entry->second->call(*this);
			++m_currentInstructionIndex;
			break;
		}
	case EInstruction::Ret:
		// Return from script function
		if (m_callStack.empty())
		{
			// Empty stack indicates either error state or end of script
			return false;
		}
		m_currentFunctionIndex = m_callStack.top().functionIndex;
		m_currentInstructionIndex = m_callStack.top().instructionIndex;
		m_callStack.pop();
		break;
	case EInstruction::Add:
		{
			CValue x;
			// 2 Values needed
			if (!popValue(x) || m_runtimeStack.empty())
			{
				return false;
			}
			m_runtimeStack.back() += x;
			++m_currentInstructionIndex;
		}
		break;
	case EInstruction::Sub:
		{
			CValue x;
			// 2 Values needed
			if (!popValue(x) || m_runtimeStack.empty())
			{
				return false;
			}
			m_runtimeStack.back() -= x;
			++m_currentInstructionIndex;
		}
		break;
	case EInstruction::Mul:
		// Not implemented
		return false;
		break;
	case EInstruction::Div:
		// Not implemented
		return false;
		break;
	case EInstruction::Inc:
		// Not implemented
		return false;
		break;
	case EInstruction::Dec:
		// Not implemented
		return false;
		break;

	case EInstruction::And:
		// Not implemented
		return false;
		break;
	case EInstruction::Or:
		// Not implemented
		return false;
		break;
	case EInstruction::Not:
		// Not implemented
		return false;
		break;
	case EInstruction::Xor:
		// Not implemented
		return false;
		break;

	case EInstruction::Jmp:
		// Not implemented
		return false;
		break;
	case EInstruction::Je:
		// Not implemented
		return false;
		break;
	case EInstruction::Jne:
		// Not implemented
		return false;
		break;
	case EInstruction::Jle:
		{
			// Compare top 2 values from stack, x <= y
			CValue y;
			CValue x;
			if (!popValue(y) || !popValue(x))
			{
				// Not enough values on stack
				return false;
			}
			if (x <= y)
			{
				// Arg 0 is target instruction index for jump
				uint32_t jumpIndex = *((uint32_t*) &instruction.args[0]);
				m_currentInstructionIndex = jumpIndex;
			}
			else
			{
				++m_currentInstructionIndex;
			}
		}
		break;
	}

	return true;
}

void CVirtualMachine::pushValue(const CValue& val)
{
	m_runtimeStack.push_back(val);
}

bool CVirtualMachine::popValue(CValue& val)
{
	if (m_runtimeStack.empty())
	{
		return false;
	}
	val = std::move(m_runtimeStack.back());
	m_runtimeStack.pop_back();
	return true;
}