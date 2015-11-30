#include "CAssembler.h"

#include <sstream>
#include <iostream>
#include <unordered_map>
#include <tuple>
#include <cstring>

CAssembler::CAssembler()
{
  // Assembler specific
  m_lexer.addKeyword("func");
  m_lexer.addKeyword("extern");
  // TODO Not supported for now
  // m_lexer.addKeyword("module");
  m_lexer.addKeyword(
      "param"); // Function parameter for implicit calling convention
  m_lexer.addKeyword("var");  // Variable
  m_lexer.addKeyword("vara"); // Variable array
  m_lexer.addKeyword("label");

  // Instructions
  m_lexer.addKeyword("nop");

  m_lexer.addKeyword("movv");
  m_lexer.addKeyword("movi");
  m_lexer.addKeyword("movf");
  m_lexer.addKeyword("movs");

  m_lexer.addKeyword("pushv");
  m_lexer.addKeyword("pushi");
  m_lexer.addKeyword("pushf");

  m_lexer.addKeyword("pop");
  m_lexer.addKeyword("popv");

  m_lexer.addKeyword("call");
  m_lexer.addKeyword("calle");

  m_lexer.addKeyword("ret");
  m_lexer.addKeyword("retv");
  m_lexer.addKeyword("reti");

  m_lexer.addKeyword("add");
  m_lexer.addKeyword("sub");

  m_lexer.addKeyword("jle");

  // Disable emitting of newline token
  m_lexer.ignoreNewLine(true);
}

bool CAssembler::assemble(std::istream &asmCode, std::ostream &byteCode)
{
  m_functions.clear();
  m_externFunctions.clear();
  m_strings.clear();

  // Parse code
  do
  {
    m_lexer.lex(asmCode);

    // At the top level of the assembly code are allowed
    // - extern function declares, which hold name and parameter count
    // information
    // - function declare/definition, groups assembly instructions for the
    // script function
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
      else if (m_lexer.getLexeme() == "module")
      {
        // Module declaration
        if (!parseModule(asmCode))
        {
          std::cout << "Parse module definition failed." << std::endl;
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
      std::cout << "Invalid token: " << m_lexer.getLexeme() << ", "
                << (int)m_lexer.getToken() << std::endl;
      return false;
    }
  } while (m_lexer.getToken() != ELexerToken::End);

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

bool CAssembler::parseFunction(std::istream &stream)
{
  // 'func' keyword already parsed, next is identifier
  m_lexer.lex(stream);
  if (m_lexer.getToken() != ELexerToken::Identifier)
  {
    return false;
  }

  // Stores labels for resolving jumps
  std::unordered_map<std::string, uint32_t> labels;
  // Stores variables with relative stack address
  std::unordered_map<std::string, uint32_t> variables;
  // Required stack size for variables
  uint32_t stackSize = 0;
  // Required stack size for passed parameters
  uint32_t parameterSize = 0;
  // Stores jump instructions to be resolved after end of pass
  std::list<std::tuple<SInstruction *, std::string>> unresolvedJumps;

  // Store function name
  SFunction tempFunction;
  tempFunction.name = m_lexer.getLexeme();

  // Store function to resolve recursive calls
  m_functions.push_back(tempFunction);
  SFunction &function = m_functions.back();

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
      if (m_lexer.getLexeme() == "param")
      {
        // Parameter for function
        // Expects identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }
        // Parameter name
        std::string paramName = m_lexer.getLexeme();

        // Set parameter offset to current stack size
        variables[paramName] = stackSize;

        // Increment parameter stack size by one for single parameter declare
        parameterSize += 1;
        // Increment required function stack size
        stackSize += 1;
      }
      else if (m_lexer.getLexeme() == "label")
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
          std::cout << "The label '" << m_lexer.getLexeme()
                    << "' already exists." << std::endl;
          return false;
        }
        // Point to next instruction -> set to current size
        labels[m_lexer.getLexeme()] = function.instructions.size();
      }
      else if (m_lexer.getLexeme() == "var")
      {
        // Variable declare
        // Expects identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }
        // Variable name
        std::string varName = m_lexer.getLexeme();

        // Set variable offset to current stack size
        variables[varName] = stackSize;

        // Increment stack size by one for single variable declare
        stackSize += 1;
      }
      else if (m_lexer.getLexeme() == "vara")
      {
        // Variable array declare
        // Expects identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }
        // Variable name
        std::string varName = m_lexer.getLexeme();

        // Expect integer array size
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Integer)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Convert to array size
        uint32_t size = 1;
        std::stringstream ss(m_lexer.getLexeme());
        ss >> size;
        if (size == 0)
        {
          std::cout << "Invalid array size." << std::endl;
          return false;
        }

        // Set variable array offset to current stack size
        variables[varName] = stackSize;
        // Increment stack size by array size
        stackSize += size;
      }
      else if (m_lexer.getLexeme() == "nop")
      {
        // No operation
        instruction.id = EInstruction::Nop;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "exit")
      {
        // Exit script immediately
        instruction.id = EInstruction::Exit;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movv")
      {
        // Move variable to variable instruction
        instruction.id = EInstruction::Movv;

        // Expects 2 identifier as arguments, separated by ',' token
        // First identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Comma)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Second identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry2 = variables.find(m_lexer.getLexeme());
        if (entry2 == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Set arg 0 to relative variable stack index of var1
        instruction.args[0] = *((int32_t *)&entry1->second);
        // Set arg 1 to relative variable stack index of var2
        instruction.args[1] = *((int32_t *)&entry2->second);

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movi")
      {
        // Move integer to variable instruction
        instruction.id = EInstruction::Movi;

        // Expects identifier and integer value as arguments, separated by ','
        // token
        // Expect identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Comma)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Expect integer
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Integer)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        int32_t value;
        std::stringstream ss(m_lexer.getLexeme());
        ss >> value;

        // Set arg 0 to relative variable stack index of var1
        instruction.args[0] = *((int32_t *)&entry1->second);
        // Set arg 1 to integer value
        instruction.args[1] = value;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movf")
      {
        // Move float to variable instruction
        instruction.id = EInstruction::Movf;

        // Expects identifier and float value as arguments, separated by ','
        // token
        // Expect identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Comma)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Expect float
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Float)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Convert to float
        // TODO Slow!!
        float value;
        std::stringstream ss(m_lexer.getLexeme());
        ss >> value;

        // Set arg 0 to relative variable stack index of var1
        instruction.args[0] = *((int32_t *)&entry1->second);
        // Set arg 1 to float value
        instruction.args[1] = *((int32_t *)&value);

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movs")
      {
        // Move string to variable instruction
        instruction.id = EInstruction::Movs;

        // Expects identifier and string value as arguments, separated by ','
        // token
        // Expect identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Comma)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Expect string
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::String)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Add string to string table
        uint32_t strIndex = addString(m_lexer.getLexeme());

        // Set arg 0 to relative variable stack index of var1
        instruction.args[0] = *((int32_t *)&entry1->second);
        // Set arg 1 to string index
        instruction.args[1] = *((int32_t *)&strIndex);

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushv")
      {
        // Push variable instruction
        instruction.id = EInstruction::Pushv;
        // Expects one identifiet as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry = variables.find(m_lexer.getLexeme());
        if (entry == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Set arg 0 to relative variable stack index
        instruction.args[0] = *((int32_t *)&entry->second);

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushi")
      {
        // Push integer instruction
        instruction.id = EInstruction::Pushi;
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
        instruction.id = EInstruction::Pushf;
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
        memcpy((void *)&instruction.args[0], (void *)&temp, sizeof(float));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushs")
      {
        // Push string instruction
        instruction.id = EInstruction::Pushs;
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
      else if (m_lexer.getLexeme() == "pop")
      {
        // Pop variable instruction
        instruction.id = EInstruction::Pop;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "popv")
      {
        // Pop variable instruction
        instruction.id = EInstruction::Popv;
        // Expects one identifier as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry = variables.find(m_lexer.getLexeme());
        if (entry == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Set arg 0 to relative variable stack index
        instruction.args[0] = *((int32_t *)&entry->second);

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "call")
      {
        // Call script function instruction
        instruction.id = EInstruction::Call;
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
          std::cout << "The identifier is not a function: "
                    << m_lexer.getLexeme() << std::endl;
          return false;
        }
        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "calle")
      {
        // Call extern function instruction
        instruction.id = EInstruction::Calle;
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
          std::cout << "The identifier is not an extern function: "
                    << m_lexer.getLexeme() << std::endl;
          return false;
        }
        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "ret")
      {
        // Return from function
        instruction.id = EInstruction::Ret;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "retv")
      {
        // Return variable from function
        instruction.id = EInstruction::Retv;

        // Expects one identifier as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != ELexerToken::Identifier)
        {
          std::cout << "Unexpected token: " << m_lexer.getLexeme() << std::endl;
          return false;
        }

        // Identifier must be valid variable name
        auto entry = variables.find(m_lexer.getLexeme());
        if (entry == variables.end())
        {
          std::cout << "Unknown variable name: " << m_lexer.getLexeme()
                    << std::endl;
          return false;
        }

        // Set arg 0 to relative variable stack index
        instruction.args[0] = *((int32_t *)&entry->second);

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "reti")
      {
        // Return integer from function
        instruction.id = EInstruction::Reti;
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
      else if (m_lexer.getLexeme() == "add")
      {
        // Add top 2 parameters from stack and push result to stack
        instruction.id = EInstruction::Add;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "sub")
      {
        // Subtract top 2 parameters from stack and push result to stack
        instruction.id = EInstruction::Sub;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "jle")
      {
        // Compare top 2 parameters from stack and jump to label if less or
        // equal
        instruction.id = EInstruction::Jle;
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
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      break;
    case ELexerToken::Comment:
    case ELexerToken::NewLine:
    case ELexerToken::CloseBrace:
      // Ignore
      break;
    default:
      std::cout << "Invalid token: '" << m_lexer.getLexeme() << "', "
                << (int)m_lexer.getToken() << std::endl;
      return false;
    }
  } while (m_lexer.getToken() != ELexerToken::CloseBrace);

  // Set stack size needed for local variables
  function.stackSize = stackSize;
  function.parameterSize = parameterSize;

  // Resolve jumps
  for (auto &entry : unresolvedJumps)
  {
    auto label = labels.find(std::get<1>(entry));
    if (label == labels.end())
    {
      std::cout << "Unresolved jump instruction: Label '" << std::get<1>(entry)
                << "' does not exist." << std::endl;
      return false;
    }
    // Set instruction arg 0 to target instruction index
    // TODO Check for infinite loops?
    std::get<0>(entry)->args[0] = label->second;
  }

  return true;
}

bool CAssembler::parseExternFunction(std::istream &stream)
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

bool CAssembler::parseModule(std::istream &stream)
{
  // 'module' keyword already parsed, next is identifier
  m_lexer.lex(stream);
  if (m_lexer.getToken() != ELexerToken::Identifier)
  {
    return false;
  }

  // Check for multiple module declares
  if (!m_header.moduleName.empty())
  {
    std::cout << "Multiple 'module' declarations encountered. A script may "
                 "have only one module declaration." << std::endl;
    return false;
  }
  // Set module in header
  m_header.moduleName = m_lexer.getLexeme();
  return true;
}

bool CAssembler::isFunction(const std::string &name)
{
  int32_t id;
  return getFunctionId(name, id);
}

bool CAssembler::getFunctionId(const std::string &name, int32_t &id)
{
  // Linear search
  // TODO Slow?
  id = 0;
  for (const auto &function : m_functions)
  {
    if (function.name == name)
    {
      return true;
    }
    ++id;
  }
  return false;
}

bool CAssembler::isExternFunction(const std::string &name)
{
  int32_t id;
  return getExternFunctionId(name, id);
}

bool CAssembler::getExternFunctionId(const std::string &name, int32_t &id)
{
  // Linear search
  // TODO Slow?
  id = 0;
  for (const auto &externFunction : m_externFunctions)
  {
    if (externFunction.name == name)
    {
      return true;
    }
    ++id;
  }
  return false;
}

bool CAssembler::serializeStrings(std::ostream &stream) const
{
  // Write string count
  uint32_t size = m_strings.size();
  stream.write((char *)&size, 4);

  // Serialize strings
  for (const auto &str : m_strings)
  {
    // Write size
    uint32_t length = str.length();
    stream.write((char *)&length, 4);
    // Write data
    stream.write(str.data(), length);
  }
  return stream.good();
}

bool CAssembler::serializeExternFunctions(std::ostream &stream) const
{
  // Write extern function count
  uint32_t size = m_externFunctions.size();
  stream.write((char *)&size, 4);

  // Serialize extern functions
  for (const auto &externFunction : m_externFunctions)
  {
    // Serialize extern function
    if (!serialize(externFunction, stream))
    {
      return false;
    }
  }
  return stream.good();
}

bool CAssembler::serializeFunctions(std::ostream &stream) const
{
  // Write function count
  uint32_t size = m_functions.size();
  stream.write((char *)&size, sizeof(size));

  // Serialize functions
  for (const auto &function : m_functions)
  {
    // Write name
    // Write size
    uint32_t length = function.name.length();
    stream.write((char *)&length, sizeof(length));
    // Write data
    stream.write(function.name.data(), length);

    // Write stack size
    stream.write((char *)&function.stackSize, sizeof(function.stackSize));
    // Write parameter size
    stream.write((char *)&function.parameterSize,
                 sizeof(function.parameterSize));

    // Write instructions
    // Write instruction size
    size = function.instructions.size();
    stream.write((char *)&size, sizeof(size));

    for (const auto &instruction : function.instructions)
    {
      if (!serialize(instruction, stream))
      {
        return false;
      }
    }
  }
  return stream.good();
}

uint32_t CAssembler::addString(const std::string &str)
{
  // Add string to table if it does not exist
  uint32_t index = 0;
  for (const auto &str : m_strings)
  {
    if (str == m_lexer.getLexeme())
    {
      return index;
    }
    ++index;
  }
  // Not found
  m_strings.push_back(str);
  return m_strings.size() - 1;
}
