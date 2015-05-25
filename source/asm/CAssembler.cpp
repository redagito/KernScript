#include "CAssembler.h"

#include <sstream>
#include <iostream>
#include <unordered_map>
#include <tuple>

CAssembler::CAssembler()
{
	// Assembler specific
	m_lexer.addKeyword("func");
	m_lexer.addKeyword("extern");
	m_lexer.addKeyword("label");

	// Instructions
	m_lexer.addKeyword("pushi");
	m_lexer.addKeyword("pushf");
	
	m_lexer.addKeyword("call");
	m_lexer.addKeyword("calle");
	m_lexer.addKeyword("ret");

	m_lexer.addKeyword("add");
	m_lexer.addKeyword("sub");

	m_lexer.addKeyword("jle");

	// Disable emitting of newline token
	m_lexer.ignoreNewLine(true);
}

bool CAssembler::assemble(std::istream& asmCode, std::ostream& byteCode)
{
	m_functions.clear();
	m_externFunctions.clear();
	m_strings.clear();

	// Parse code
	do
	{
		m_lexer.lex(asmCode);
		
		// At the top level of the assembly code are allowed
		// - extern function declares, which hold name and parameter count information
		// - function declare/definition, groups assembly instructions for the script function
		// - comments, which are ignored
		switch (m_lexer.getToken())
		{
		case ELexerToken::Keyword:
			if (m_lexer.getLexeme() == "func")
			{
				// Script function
				if (!parseFunction(asmCode))
				{
					std::cout << "Parse function failed." << std::endl;
					return false;
				}
			}
			else if (m_lexer.getLexeme() == "extern")
			{
				// Extern function
				if (!parseExternFunction(asmCode))
				{
					std::cout << "Parse extern function failed." << std::endl;
					return false;
				}
			}
			break;
		case ELexerToken::Comment:
		case ELexerToken::NewLine:
		case ELexerToken::End:
			// Ignore
			break;
		default:
			// Invalid token
			std::cout << "Invalid token: " << m_lexer.getLexeme() << ", " << (int) m_lexer.getToken() << std::endl;			
			return false;
		}
	}
	while (m_lexer.getToken() != ELexerToken::End);
	

	std::cout << "Serializing" << std::endl;
	// Serialize to byte code
	if (!serializeStrings(byteCode))
	{
		std::cout << "Serialize strings failed" << std::endl;
		return false;
	}
	if (!serializeExternFunctions(byteCode))
	{
		std::cout << "Serialize extern functions failed" << std::endl;
		return false;
	}
	if (!serializeFunctions(byteCode))
	{
		std::cout << "Serialize functions failed" << std::endl;
		return false;
	}
	
	return true;
}

bool CAssembler::parseFunction(std::istream& stream)
{
	// 'func' keyword already parsed, next is identifier
	m_lexer.lex(stream);
	if (m_lexer.getToken() != ELexerToken::Identifier)
	{
		return false;
	}
	
	// Store labels for resolving jumps
	std::unordered_map<std::string, uint32_t> labels;
	// Stores jump instructions to be resolved after end of pass
	std::list<std::tuple<SInstruction*, std::string>> unresolvedJumps;

	// Store function name
	SFunction function;
	function.name = m_lexer.getLexeme();
	
	// Next is '{'
	m_lexer.lex(stream);
	if (m_lexer.getToken() != ELexerToken::OpenBrace)
	{
		return false;
	}
	
	do
	{
		SInstruction instruction;
		m_lexer.lex(stream);
		switch (m_lexer.getToken())
		{
		case ELexerToken::Keyword:
			if (m_lexer.getLexeme() == "label")
			{
				// Instruction index alias for jumps
				// Expects identifier for label name
				m_lexer.lex(stream);
				if (m_lexer.getToken() != ELexerToken::Identifier)
				{
					std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// Check if label already exists
				if (labels.find(m_lexer.getLexeme()) != labels.end())
				{
					std::cout << "The label '" << m_lexer.getLexeme() << "' already exists." << std::endl;
				}
				// Point to next instruction -> set to current size
				labels[m_lexer.getLexeme()] = function.instructions.size();
			}
			else if (m_lexer.getLexeme() == "pushi")
			{
				// Push integer instruction
				instruction.id = EInstructíon::Pushi;
				// Expects one integer constant as argument
				m_lexer.lex(stream);
				if (m_lexer.getToken() != ELexerToken::Integer)
				{
					std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// TODO Slow!!!
				std::stringstream ss;
				ss << m_lexer.getLexeme();
				ss >> instruction.args[0];

				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "pushf")
			{
				// Push float instruction
				instruction.id = EInstructíon::Pushf;
				// Expects one float constant as argument
				m_lexer.lex(stream);
				if (m_lexer.getToken() != ELexerToken::Float)
				{
					std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// TODO Slow!!!
				std::stringstream ss;
				ss << m_lexer.getLexeme();
				float temp;
				ss >> temp;
				// Store float binary data in arg
				memcpy((void*)&instruction.args[0], (void*)&temp, sizeof(float));

				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "pushs")
			{
				// Push string instruction
				instruction.id = EInstructíon::Pushs;
				// Expects one string constant as argumment
				m_lexer.lex(stream);
				if (m_lexer.getToken() != ELexerToken::String)
				{
					std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// TODO Add string to strings list and write id
				return false;

				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "call")
			{
				// Call script function instruction
				instruction.id = EInstructíon::Call;
				// Expects one identifier
				m_lexer.lex(stream);
				if (m_lexer.getToken() != ELexerToken::Identifier)
				{
					std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// Identifier must be script function
				// Set function index
				if (!getFunctionId(m_lexer.getLexeme(), instruction.args[0]))
				{
					std::cout << "The identifier is not a function: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "calle")
			{
				// Call extern function instruction
				instruction.id = EInstructíon::Calle;
				// Expects one identifier
				m_lexer.lex(stream);
				if (m_lexer.getToken() != ELexerToken::Identifier)
				{
					std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// Identifier must be extern function
				// Set function index
				if (!getExternFunctionId(m_lexer.getLexeme(), instruction.args[0]))
				{
					std::cout << "The identifier is not an extern function: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "ret")
			{
				// Return from function
				instruction.id = EInstructíon::Ret;
				// No arguments

				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "add")
			{
				// Add top 2 parameters from stack and push result to stack
				instruction.id = EInstructíon::Add;
				// No arguments

				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "sub")
			{
				// Subtract top 2 parameters from stack and push result to stack
				instruction.id = EInstructíon::Sub;
				// No arguments

				// Add assembled instruction
				function.instructions.push_back(instruction);
			}
			else if (m_lexer.getLexeme() == "jle")
			{
				// Compare top 2 parameters from stack and jump to label if less or equal
				instruction.id = EInstructíon::Jle;
				// Expects identifier for label name as argument
				m_lexer.lex(stream);
				if (m_lexer.getToken() != ELexerToken::Identifier)
				{
					std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
					return false;
				}

				// Add assembled instruction
				function.instructions.push_back(instruction);

				// Add instruction to unresolved jumps
				unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(), m_lexer.getLexeme()));
			}
			break;
		case ELexerToken::Comment:
		case ELexerToken::NewLine:
		case ELexerToken::CloseBrace:
			// Ignore
			break;
		default:
			std::cout << "Invalid token: '" << m_lexer.getLexeme() << "', " << (int)m_lexer.getToken() << std::endl;
			return false;
		}
	}
	while (m_lexer.getToken() != ELexerToken::CloseBrace);
	
	// Resolve jumps
	for (auto& entry : unresolvedJumps)
	{
		auto label = labels.find(std::get<1>(entry));
		if (label == labels.end())
		{
			std::cout << "Unresolved jump instruction: Label '" << std::get<1>(entry) << "' does not exist." << std::endl;
			return false;
		}
		// Set instruction arg 0 to target instruction index
		// TODO Check for infinite loops?
		std::get<0>(entry)->args[0] = label->second;
	}

	// Add assembled function
	m_functions.push_back(function);
	return true;
}

bool CAssembler::parseExternFunction(std::istream& stream)
{
	// 'extern* keyword already parsed, next is identifier
	m_lexer.lex(stream);
	if (m_lexer.getToken() != ELexerToken::Identifier)
	{
		return false;
	}
	
	// Create extern function and store name
	SExternFunction externFunction;
	externFunction.name = m_lexer.getLexeme();
	
	// Next is integer constant which denotes the number of expected arguments
	m_lexer.lex(stream);
	if (m_lexer.getToken() != ELexerToken::Integer)
	{
		return false;
	}
	// String to int convert
	// TODO SLOW!!
	std::stringstream ss;
	ss << m_lexer.getLexeme();
	int temp;
	ss >> temp;
	externFunction.argCount = temp;
	
	// Add extern function
	m_externFunctions.push_back(externFunction);
	return true;
}


bool CAssembler::isFunction(const std::string& name)
{
	int32_t id;
	return getFunctionId(name, id);
}

bool CAssembler::getFunctionId(const std::string& name, int32_t& id)
{
	// Linear search
	// TODO Slow?
	id = 0;
	for (const auto& function : m_functions)
	{
		if (function.name == name)
		{
			return true;
		}
		++id;
	}
	return false;
}

bool CAssembler::isExternFunction(const std::string& name)
{
	int32_t id;
	return getExternFunctionId(name, id);
}

bool CAssembler::getExternFunctionId(const std::string& name, int32_t& id)
{
	// Linear search
	// TODO Slow?
	id = 0;
	for (const auto& externFunction : m_externFunctions)
	{
		if (externFunction.name == name)
		{
			return true;
		}
		++id;
	}
	return false;
}

bool CAssembler::serializeStrings(std::ostream& stream) const
{
	// Write string count
	uint32_t size = m_strings.size();
	stream.write((char*)&size, 4);
	
	// Serialize strings
	for (const auto& str : m_strings)
	{
		// Write size
		uint32_t length = str.length();
		stream.write((char*)&length, 4);
		// Write data
		stream.write(str.data(), length);
	}
	return stream.good();
}

bool CAssembler::serializeExternFunctions(std::ostream& stream) const
{
	// Write extern function count
	uint32_t size = m_externFunctions.size();
	stream.write((char*)&size, 4);
	
	// Serialize extern functions
	for (const auto& externFunction : m_externFunctions)
	{
		// Serialize extern function
		if (!serialize(externFunction, stream))
		{
			return false;
		}
	}
	return stream.good();
}

bool CAssembler::serializeFunctions(std::ostream& stream) const
{
	// Write function count
	uint32_t size = m_functions.size();
	stream.write((char*)&size, sizeof(size));
	
	// Serialize functions
	for (const auto& function : m_functions)
	{
		// Write name
		// Write size
		uint32_t length = function.name.length();
		stream.write((char*)&length, sizeof(length));
		// Write data
		stream.write(function.name.data(), length);
		
		// Write instructions
		// Write instruction size
		size = function.instructions.size();
		stream.write((char*)&size, sizeof(size));
		
		for (const auto& instruction : function.instructions)
		{
			if (!serialize(instruction, stream))
			{
				return false;
			}
		}
	}
	return stream.good();
}