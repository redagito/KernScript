#include "CAssembler.h"

#include <sstream>
#include <iostream>

CAssembler::CAssembler()
{
	m_lexer.addKeyword("func");
	m_lexer.addKeyword("extern");
	m_lexer.addKeyword("calle");
	m_lexer.addKeyword("call");
	m_lexer.addKeyword("pushi");
	m_lexer.addKeyword("pushf");
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
			if (m_lexer.getLexeme() == "pushi")
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
			}
			if (m_lexer.getLexeme() == "pushf")
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
				memcpy((void*) &instruction.args[0], (void*) &temp, sizeof(float));
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
				if (!isExternFunction(m_lexer.getLexeme()))
				{
					std::cout << "The identifier is not an extern function: " << m_lexer.getLexeme() << std::endl;
					return false;
				}
				// Set function index
				instruction.args[0] = getExternFunctionId(m_lexer.getLexeme());
			}
			// Add assembled instruction
			function.instructions.push_back(instruction);
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

bool CAssembler::isExternFunction(const std::string& name)
{
	// Linear search
	// TODO Slow?
	for (const auto& externFunction : m_externFunctions)
	{
		if (externFunction.name == name)
		{
			return true;
		}
	}
	return false;
}

int32_t CAssembler::getExternFunctionId(const std::string& name)
{
	// Linear search
	// TODO Slow?
	int32_t index = 0;
	for (const auto& externFunction : m_externFunctions)
	{
		if (externFunction.name == name)
		{
			return index;
		}
		++index;
	}
	return -1;
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