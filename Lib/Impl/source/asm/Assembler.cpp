#include "Assembler.h"

#include <sstream>
#include <unordered_map>
#include <tuple>
#include <cstring>
#include <limits>

#include "logging/Logger.h"

#include "common/Version.h"

namespace
{

/**
* \brief Parses an assembler integer literal with full validation.
*
* The lexer only produces digit runs for Integer tokens, but a run can exceed
* the target type. An unchecked stringstream extraction leaves the target
* untouched on failure, so the caller would silently keep a default or
* indeterminate value. Checking the stream state rejects such literals.
*/
template <typename T> bool parseInteger(const std::string &text, T &value)
{
  std::stringstream stream(text);
  stream >> value;
  return !stream.fail() && stream.eof();
}

/**
* \brief Parses an assembler float magnitude with full validation.
*
* The magnitude widens through double so that an out-of-range literal fails
* the range check instead of leaving an indeterminate float behind. The sign
* is folded in by the caller, whose magnitude is symmetric around zero.
*/
bool parseFloat(const std::string &text, float &value)
{
  std::stringstream stream(text);
  double magnitude = 0.0;
  stream >> magnitude;
  if (stream.fail() || !stream.eof())
  {
    return false;
  }
  if (magnitude > static_cast<double>(std::numeric_limits<float>::max()))
  {
    return false;
  }
  value = static_cast<float>(magnitude);
  return true;
}

}

Assembler::Assembler(kern::ILogger *logger)
    : m_lexer(logger), m_logger(logger != nullptr ? logger : nullLogger())
{
  // Assembler specific
  m_lexer.addKeyword("func");
  m_lexer.addKeyword("extern");
  m_lexer.addKeyword("module");
  m_lexer.addKeyword(
      "param"); // Function parameter for implicit calling convention
  m_lexer.addKeyword("var");  // Variable
  m_lexer.addKeyword("vara"); // Variable array
  m_lexer.addKeyword("label");

  // Instructions
  m_lexer.addKeyword("nop");
  m_lexer.addKeyword("break");
  m_lexer.addKeyword("exit");

  m_lexer.addKeyword("movv");
  m_lexer.addKeyword("movi");
  m_lexer.addKeyword("movf");
  m_lexer.addKeyword("movs");

  m_lexer.addKeyword("pushv");
  m_lexer.addKeyword("pushi");
  m_lexer.addKeyword("pushu");
  m_lexer.addKeyword("pushf");
  m_lexer.addKeyword("pushs");

  m_lexer.addKeyword("pop");
  m_lexer.addKeyword("popv");

  m_lexer.addKeyword("call");
  m_lexer.addKeyword("calle");

  m_lexer.addKeyword("ret");
  m_lexer.addKeyword("retv");
  m_lexer.addKeyword("reti");
  m_lexer.addKeyword("retf");
  m_lexer.addKeyword("rets");

  m_lexer.addKeyword("add");
  m_lexer.addKeyword("sub");
  m_lexer.addKeyword("mul");
  m_lexer.addKeyword("div");
  m_lexer.addKeyword("inc");
  m_lexer.addKeyword("dec");

  m_lexer.addKeyword("and");
  m_lexer.addKeyword("or");
  m_lexer.addKeyword("not");
  m_lexer.addKeyword("xor");

  m_lexer.addKeyword("jmp");
  m_lexer.addKeyword("je");
  m_lexer.addKeyword("jne");
  m_lexer.addKeyword("jg");
  m_lexer.addKeyword("jge");
  m_lexer.addKeyword("jl");
  m_lexer.addKeyword("jle");

  // Disable emitting of newline token
  m_lexer.ignoreNewLine(true);
}

bool Assembler::assemble(std::istream &asmCode, std::ostream &byteCode)
{
  m_functions.clear();
  m_externFunctions.clear();
  m_strings.clear();
  m_unresolvedCalls.clear();
  m_unresolvedExternCalls.clear();

  // Fresh header for this assembly; the module declaration fills in the
  // module name while parsing.
  m_header.identifier[0] = 'K';
  m_header.identifier[1] = 'E';
  m_header.identifier[2] = 'R';
  m_header.identifier[3] = 'N';
  m_header.majorVersion = static_cast<uint8_t>(majorVersion);
  m_header.minorVersion = static_cast<uint8_t>(minorVersion);
  m_header.moduleName.clear();

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
    case LexerToken::Keyword:
      if (m_lexer.getLexeme() == "func")
      {
        // Script function
        if (!parseFunction(asmCode))
        {
          m_logger->error("Parse function failed.");
          return false;
        }
      }
      else if (m_lexer.getLexeme() == "extern")
      {
        // Extern function
        if (!parseExternFunction(asmCode))
        {
          m_logger->error("Parse extern function failed.");
          return false;
        }
      }
      else if (m_lexer.getLexeme() == "module")
      {
        // Module declaration
        if (!parseModule(asmCode))
        {
          m_logger->error("Parse module definition failed.");
          return false;
        }
      }
      break;
    case LexerToken::Comment:
    case LexerToken::NewLine:
    case LexerToken::End:
      // Ignore
      break;
    default:
      // Invalid token
      m_logger->error("Invalid token: {}, {}", m_lexer.getLexeme(), (int)m_lexer.getToken());
      return false;
    }
  } while (m_lexer.getToken() != LexerToken::End);

  // Resolve deferred calls now that every function and extern is known, so
  // forward references and mutual recursion assemble.
  for (auto &entry : m_unresolvedCalls)
  {
    if (!getFunctionId(std::get<1>(entry), std::get<0>(entry)->args[0]))
    {
      m_logger->error("The identifier is not a function: {}",
                      std::get<1>(entry));
      return false;
    }
  }
  for (auto &entry : m_unresolvedExternCalls)
  {
    if (!getExternFunctionId(std::get<1>(entry),
                             std::get<0>(entry)->args[0]))
    {
      m_logger->error("The identifier is not an extern function: {}",
                      std::get<1>(entry));
      return false;
    }
  }

  // The header leads the bytecode, so load() validates magic and version
  // before reading any table.
  if (!serialize(m_header, byteCode))
  {
    m_logger->error("Serialize header failed");
    return false;
  }
  // Serialize to byte code
  if (!serializeStrings(byteCode))
  {
    m_logger->error("Serialize strings failed");
    return false;
  }
  if (!serializeExternFunctions(byteCode))
  {
    m_logger->error("Serialize extern functions failed");
    return false;
  }
  if (!serializeFunctions(byteCode))
  {
    m_logger->error("Serialize functions failed");
    return false;
  }

  return true;
}

bool Assembler::parseFunction(std::istream &stream)
{
  // 'func' keyword already parsed, next is identifier
  m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::Identifier)
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
  std::list<std::tuple<Instruction *, std::string>> unresolvedJumps;

  // Store function name
  Function tempFunction;
  tempFunction.name = m_lexer.getLexeme();

  // A function may call itself, but redefining one only shadows the first.
  if (isFunction(tempFunction.name))
  {
    m_logger->error("The function '{}' already exists.", tempFunction.name);
    return false;
  }

  // Store function to resolve recursive calls
  m_functions.push_back(tempFunction);
  Function &function = m_functions.back();

  // Next is '{'
  m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::OpenBrace)
  {
    return false;
  }

  do
  {
    Instruction instruction;
    m_lexer.lex(stream);
    switch (m_lexer.getToken())
    {
    case LexerToken::Keyword:
      if (m_lexer.getLexeme() == "param")
      {
        // Parameter for function
        // Expects identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // Parameter name
        std::string paramName = m_lexer.getLexeme();

        // Parameters share the variable namespace, so redeclaring one only
        // shadows the first slot.
        if (variables.find(paramName) != variables.end())
        {
          m_logger->error("The variable '{}' already exists.", paramName);
          return false;
        }

        if (stackSize == UINT32_MAX || parameterSize == UINT32_MAX)
        {
          m_logger->error("Function frame size overflow.");
          return false;
        }

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
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // Check if label already exists
        if (labels.find(m_lexer.getLexeme()) != labels.end())
        {
          m_logger->error("The label '{}' already exists.", m_lexer.getLexeme());
          return false;
        }
        // Point to next instruction -> set to current size
        labels[m_lexer.getLexeme()] = static_cast<uint32_t>(function.instructions.size());
      }
      else if (m_lexer.getLexeme() == "var")
      {
        // Variable declare
        // Expects identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // Variable name
        std::string varName = m_lexer.getLexeme();

        if (variables.find(varName) != variables.end())
        {
          m_logger->error("The variable '{}' already exists.", varName);
          return false;
        }

        if (stackSize == UINT32_MAX)
        {
          m_logger->error("Function frame size overflow.");
          return false;
        }

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
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // Variable name
        std::string varName = m_lexer.getLexeme();

        if (variables.find(varName) != variables.end())
        {
          m_logger->error("The variable '{}' already exists.", varName);
          return false;
        }

        // Expect integer array size
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Integer)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Convert to array size
        uint32_t size = 0;
        if (!parseInteger(m_lexer.getLexeme(), size) || size == 0)
        {
          m_logger->error("Invalid array size: {}", m_lexer.getLexeme());
          return false;
        }

        // The addition below must not wrap, or the frame would come out
        // smaller than the declared variables.
        if (size > UINT32_MAX - stackSize)
        {
          m_logger->error("Function frame size overflow.");
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
        instruction.id = Opcode::Nop;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "break")
      {
        // Breakpoint, for debugging. There is no debugger attached, so this
        // intentionally does nothing and execution continues.
        instruction.id = Opcode::Break;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "exit")
      {
        // Exit script immediately
        instruction.id = Opcode::Exit;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movv")
      {
        // Move variable to variable instruction
        instruction.id = Opcode::Movv;

        // Expects 2 identifier as arguments, separated by ',' token
        // First identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Comma)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Second identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry2 = variables.find(m_lexer.getLexeme());
        if (entry2 == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Set arg 0 to relative variable stack index of var1
        memcpy(&instruction.args[0], &entry1->second,
               sizeof(instruction.args[0]));
        // Set arg 1 to relative variable stack index of var2
        memcpy(&instruction.args[1], &entry2->second,
               sizeof(instruction.args[1]));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movi")
      {
        // Move integer to variable instruction
        instruction.id = Opcode::Movi;

        // Expects identifier and integer value as arguments, separated by ','
        // token
        // Expect identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Comma)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Expect integer
        // A leading dash negates the literal. The minus is assembler-level
        // unary minus, not a lexer-level negative number, so the token
        // stream stays stable.
        m_lexer.lex(stream);
        bool moviNegative = false;
        if (m_lexer.getToken() == LexerToken::Dash)
        {
          moviNegative = true;
          m_lexer.lex(stream);
        }
        if (m_lexer.getToken() != LexerToken::Integer)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Parse the magnitude wide so that -2147483648 stays representable,
        // then fold the sign in with a range check.
        int64_t moviMagnitude = 0;
        if (!parseInteger(m_lexer.getLexeme(), moviMagnitude))
        {
          m_logger->error("Invalid integer literal: {}", m_lexer.getLexeme());
          return false;
        }
        const int64_t moviSigned =
            moviNegative ? -moviMagnitude : moviMagnitude;
        if (moviSigned < static_cast<int64_t>(INT32_MIN) ||
            moviSigned > static_cast<int64_t>(INT32_MAX))
        {
          m_logger->error("Invalid integer literal: {}", m_lexer.getLexeme());
          return false;
        }
        int32_t value = static_cast<int32_t>(moviSigned);

        // Set arg 0 to relative variable stack index of var1
        memcpy(&instruction.args[0], &entry1->second,
               sizeof(instruction.args[0]));
        // Set arg 1 to integer value
        instruction.args[1] = value;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movf")
      {
        // Move float to variable instruction
        instruction.id = Opcode::Movf;

        // Expects identifier and float value as arguments, separated by ','
        // token
        // Expect identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Comma)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Expect float, with an optional leading dash (assembler-level unary
        // minus, keeping the token stream stable)
        m_lexer.lex(stream);
        bool movfNegative = false;
        if (m_lexer.getToken() == LexerToken::Dash)
        {
          movfNegative = true;
          m_lexer.lex(stream);
        }
        if (m_lexer.getToken() != LexerToken::Float)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Convert to float
        float value = 0.f;
        if (!parseFloat(m_lexer.getLexeme(), value))
        {
          m_logger->error("Invalid float literal: {}", m_lexer.getLexeme());
          return false;
        }
        if (movfNegative)
        {
          value = -value;
        }

        // Set arg 0 to relative variable stack index of var1
        memcpy(&instruction.args[0], &entry1->second,
               sizeof(instruction.args[0]));
        // Set arg 1 to float value
        memcpy(&instruction.args[1], &value, sizeof(value));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "movs")
      {
        // Move string to variable instruction
        instruction.id = Opcode::Movs;

        // Expects identifier and string value as arguments, separated by ','
        // token
        // Expect identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry1 = variables.find(m_lexer.getLexeme());
        if (entry1 == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Expect ',' separator
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Comma)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Expect string
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::String)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add string to string table
        uint32_t strIndex = addString(m_lexer.getLexeme());

        // Set arg 0 to relative variable stack index of var1
        memcpy(&instruction.args[0], &entry1->second,
               sizeof(instruction.args[0]));
        // Set arg 1 to string index
        memcpy(&instruction.args[1], &strIndex,
               sizeof(instruction.args[1]));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushv")
      {
        // Push variable instruction
        instruction.id = Opcode::Pushv;
        // Expects one identifiet as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry = variables.find(m_lexer.getLexeme());
        if (entry == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Set arg 0 to relative variable stack index
        memcpy(&instruction.args[0], &entry->second,
               sizeof(instruction.args[0]));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushi")
      {
        // Push integer instruction
        instruction.id = Opcode::Pushi;
        // Expects one integer constant as argument, with an optional leading
        // dash (assembler-level unary minus, keeping the token stream stable)
        m_lexer.lex(stream);
        bool pushNegative = false;
        if (m_lexer.getToken() == LexerToken::Dash)
        {
          pushNegative = true;
          m_lexer.lex(stream);
        }
        if (m_lexer.getToken() != LexerToken::Integer)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        int64_t pushMagnitude = 0;
        if (!parseInteger(m_lexer.getLexeme(), pushMagnitude))
        {
          m_logger->error("Invalid integer literal: {}", m_lexer.getLexeme());
          return false;
        }
        const int64_t pushSigned =
            pushNegative ? -pushMagnitude : pushMagnitude;
        if (pushSigned < static_cast<int64_t>(INT32_MIN) ||
            pushSigned > static_cast<int64_t>(INT32_MAX))
        {
          m_logger->error("Invalid integer literal: {}", m_lexer.getLexeme());
          return false;
        }
        int32_t pushValue = static_cast<int32_t>(pushSigned);
        instruction.args[0] = pushValue;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushu")
      {
        // Push unsigned integer instruction
        instruction.id = Opcode::Pushu;
        // Expects one integer constant as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Integer)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // Read as unsigned so that values beyond the signed range are kept.
        uint32_t temp = 0;
        if (!parseInteger(m_lexer.getLexeme(), temp))
        {
          m_logger->error("Invalid integer literal: {}", m_lexer.getLexeme());
          return false;
        }
        memcpy((void *)&instruction.args[0], (void *)&temp,
               sizeof(uint32_t));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushf")
      {
        // Push float instruction
        instruction.id = Opcode::Pushf;
        // Expects one float constant as argument, with an optional leading
        // dash (assembler-level unary minus, keeping the token stream stable)
        m_lexer.lex(stream);
        bool pushfNegative = false;
        if (m_lexer.getToken() == LexerToken::Dash)
        {
          pushfNegative = true;
          m_lexer.lex(stream);
        }
        if (m_lexer.getToken() != LexerToken::Float)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // TODO Slow!!!
        float temp = 0.f;
        if (!parseFloat(m_lexer.getLexeme(), temp))
        {
          m_logger->error("Invalid float literal: {}", m_lexer.getLexeme());
          return false;
        }
        if (pushfNegative)
        {
          temp = -temp;
        }
        // Store float binary data in arg
        memcpy((void *)&instruction.args[0], (void *)&temp, sizeof(float));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pushs")
      {
        // Push string instruction
        instruction.id = Opcode::Pushs;
        // Expects one string constant as argumment
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::String)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // Add string to string table
        uint32_t strIndex = addString(m_lexer.getLexeme());
        // Set arg 0 to string index
        memcpy(&instruction.args[0], &strIndex,
               sizeof(instruction.args[0]));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "pop")
      {
        // Pop variable instruction
        instruction.id = Opcode::Pop;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "popv")
      {
        // Pop variable instruction
        instruction.id = Opcode::Popv;
        // Expects one identifier as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry = variables.find(m_lexer.getLexeme());
        if (entry == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Set arg 0 to relative variable stack index
        memcpy(&instruction.args[0], &entry->second,
               sizeof(instruction.args[0]));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "call")
      {
        // Call script function instruction
        instruction.id = Opcode::Call;
        // Expects one identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // The callee may be defined later in the script, so record the call
        // and resolve it after the full parse, like jumps.
        // Add assembled instruction
        function.instructions.push_back(instruction);
        m_unresolvedCalls.push_back(std::make_tuple(
            &function.instructions.back(), m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "calle")
      {
        // Call extern function instruction
        instruction.id = Opcode::Calle;
        // Expects one identifier
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // The extern may be declared later in the script, so record the call
        // and resolve it after the full parse, like jumps.
        // Add assembled instruction
        function.instructions.push_back(instruction);
        m_unresolvedExternCalls.push_back(std::make_tuple(
            &function.instructions.back(), m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "ret")
      {
        // Return from function
        instruction.id = Opcode::Ret;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "retv")
      {
        // Return variable from function
        instruction.id = Opcode::Retv;

        // Expects one identifier as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Identifier must be valid variable name
        auto entry = variables.find(m_lexer.getLexeme());
        if (entry == variables.end())
        {
          m_logger->error("Unknown variable name: {}", m_lexer.getLexeme());
          return false;
        }

        // Set arg 0 to relative variable stack index
        memcpy(&instruction.args[0], &entry->second,
               sizeof(instruction.args[0]));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "reti")
      {
        // Return integer from function
        instruction.id = Opcode::Reti;
        // Expects one integer constant as argument, with an optional leading
        // dash (assembler-level unary minus, keeping the token stream stable)
        m_lexer.lex(stream);
        bool retNegative = false;
        if (m_lexer.getToken() == LexerToken::Dash)
        {
          retNegative = true;
          m_lexer.lex(stream);
        }
        if (m_lexer.getToken() != LexerToken::Integer)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        int64_t retMagnitude = 0;
        if (!parseInteger(m_lexer.getLexeme(), retMagnitude))
        {
          m_logger->error("Invalid integer literal: {}", m_lexer.getLexeme());
          return false;
        }
        const int64_t retSigned = retNegative ? -retMagnitude : retMagnitude;
        if (retSigned < static_cast<int64_t>(INT32_MIN) ||
            retSigned > static_cast<int64_t>(INT32_MAX))
        {
          m_logger->error("Invalid integer literal: {}", m_lexer.getLexeme());
          return false;
        }
        int32_t retValue = static_cast<int32_t>(retSigned);
        instruction.args[0] = retValue;

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "retf")
      {
        // Return float from function
        instruction.id = Opcode::Retf;
        // Expects one float constant as argument, with an optional leading
        // dash (assembler-level unary minus, keeping the token stream stable)
        m_lexer.lex(stream);
        bool retfNegative = false;
        if (m_lexer.getToken() == LexerToken::Dash)
        {
          retfNegative = true;
          m_lexer.lex(stream);
        }
        if (m_lexer.getToken() != LexerToken::Float)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }
        // Convert to float
        float value = 0.f;
        if (!parseFloat(m_lexer.getLexeme(), value))
        {
          m_logger->error("Invalid float literal: {}", m_lexer.getLexeme());
          return false;
        }
        if (retfNegative)
        {
          value = -value;
        }

        // Set arg 0 to float value
        memcpy(&instruction.args[0], &value, sizeof(value));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "rets")
      {
        // Return string from function
        instruction.id = Opcode::Rets;
        // Expects one string constant as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::String)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add string to string table
        uint32_t strIndex = addString(m_lexer.getLexeme());

        // Set arg 0 to string index
        memcpy(&instruction.args[0], &strIndex,
               sizeof(instruction.args[0]));

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "add")
      {
        // Add top 2 parameters from stack and push result to stack
        instruction.id = Opcode::Add;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "sub")
      {
        // Subtract top 2 parameters from stack and push result to stack
        instruction.id = Opcode::Sub;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "jmp")
      {
        // Unconditional jump to label
        instruction.id = Opcode::Jmp;
        // Expects identifier for label name as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add assembled instruction
        function.instructions.push_back(instruction);

        // Add instruction to unresolved jumps
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "je")
      {
        // Compare top 2 parameters from stack and jump to label if equal
        instruction.id = Opcode::Je;
        // Expects identifier for label name as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add assembled instruction
        function.instructions.push_back(instruction);

        // Add instruction to unresolved jumps
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "jne")
      {
        // Compare top 2 parameters from stack and jump to label if not equal
        instruction.id = Opcode::Jne;
        // Expects identifier for label name as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add assembled instruction
        function.instructions.push_back(instruction);

        // Add instruction to unresolved jumps
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "jg")
      {
        // Compare top 2 parameters from stack and jump to label if greater
        instruction.id = Opcode::Jg;
        // Expects identifier for label name as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add assembled instruction
        function.instructions.push_back(instruction);

        // Add instruction to unresolved jumps
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "jge")
      {
        // Compare top 2 parameters from stack and jump to label if greater or
        // equal
        instruction.id = Opcode::Jge;
        // Expects identifier for label name as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add assembled instruction
        function.instructions.push_back(instruction);

        // Add instruction to unresolved jumps
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "jl")
      {
        // Compare top 2 parameters from stack and jump to label if less
        instruction.id = Opcode::Jl;
        // Expects identifier for label name as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add assembled instruction
        function.instructions.push_back(instruction);

        // Add instruction to unresolved jumps
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "jle")
      {
        // Compare top 2 parameters from stack and jump to label if less or
        // equal
        instruction.id = Opcode::Jle;
        // Expects identifier for label name as argument
        m_lexer.lex(stream);
        if (m_lexer.getToken() != LexerToken::Identifier)
        {
          m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
          return false;
        }

        // Add assembled instruction
        function.instructions.push_back(instruction);

        // Add instruction to unresolved jumps
        unresolvedJumps.push_back(std::make_tuple(&function.instructions.back(),
                                                  m_lexer.getLexeme()));
      }
      else if (m_lexer.getLexeme() == "mul")
      {
        // Multiply top 2 values from stack and push result to stack
        instruction.id = Opcode::Mul;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "div")
      {
        // Divide top 2 values from stack and push result to stack
        instruction.id = Opcode::Div;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "inc")
      {
        // Increment top value of the stack in place
        instruction.id = Opcode::Inc;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "dec")
      {
        // Decrement top value of the stack in place
        instruction.id = Opcode::Dec;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "and")
      {
        // Logical AND of the top 2 values of the stack, result pushed back
        instruction.id = Opcode::And;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "or")
      {
        // Logical OR of the top 2 values of the stack, result pushed back
        instruction.id = Opcode::Or;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "not")
      {
        // Logical NOT of the top value of the stack, result pushed back
        instruction.id = Opcode::Not;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else if (m_lexer.getLexeme() == "xor")
      {
        // Logical XOR of the top 2 values of the stack, result pushed back
        instruction.id = Opcode::Xor;
        // No arguments

        // Add assembled instruction
        function.instructions.push_back(instruction);
      }
      else
      {
        // A keyword with no branch here would otherwise fall through to the
        // break below and be dropped without a diagnostic, producing bytecode
        // that runs with wrong semantics.
        m_logger->error("Unhandled keyword: '{}'.", m_lexer.getLexeme());
        return false;
      }
      break;
    case LexerToken::Comment:
    case LexerToken::NewLine:
    case LexerToken::CloseBrace:
      // Ignore
      break;
    default:
      m_logger->error("Invalid token: '{}', {}", m_lexer.getLexeme(), (int)m_lexer.getToken());
      return false;
    }
  } while (m_lexer.getToken() != LexerToken::CloseBrace);

  // Set stack size needed for local variables
  function.stackSize = stackSize;
  function.parameterSize = parameterSize;

  // Resolve jumps
  for (auto &entry : unresolvedJumps)
  {
    auto label = labels.find(std::get<1>(entry));
    if (label == labels.end())
    {
      m_logger->error("Unresolved jump instruction: Label '{}' does not exist.", std::get<1>(entry));
      return false;
    }
    // Set instruction arg 0 to target instruction index
    // TODO Check for infinite loops?
    std::get<0>(entry)->args[0] = label->second;
  }

  return true;
}

bool Assembler::parseExternFunction(std::istream &stream)
{
  // 'extern* keyword already parsed, next is identifier
  m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::Identifier)
  {
    return false;
  }

  // Create extern function and store name
  ExternFunction externFunction;
  externFunction.name = m_lexer.getLexeme();

  // Redeclaring an extern only shadows the first declaration.
  if (isExternFunction(externFunction.name))
  {
    m_logger->error("The extern function '{}' already exists.",
                    externFunction.name);
    return false;
  }

  // Next is integer constant which denotes the number of expected arguments
  m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::Integer)
  {
    m_logger->error("Unexpected token: {}", m_lexer.getLexeme());
    return false;
  }
  // String to int convert, rejecting literals that do not fit.
  int temp = 0;
  if (!parseInteger(m_lexer.getLexeme(), temp))
  {
    m_logger->error("Invalid extern argument count: {}",
                    m_lexer.getLexeme());
    return false;
  }
  // argCount serializes as a single byte, so only 0..255 survives the trip.
  if (temp < 0 || temp > 255)
  {
    m_logger->error("Extern argument count out of range: {}",
                    m_lexer.getLexeme());
    return false;
  }
  externFunction.argCount = static_cast<uint8_t>(temp);

  // Add extern function
  m_externFunctions.push_back(externFunction);
  return true;
}

bool Assembler::parseModule(std::istream &stream)
{
  // 'module' keyword already parsed, next is identifier
  m_lexer.lex(stream);
  if (m_lexer.getToken() != LexerToken::Identifier)
  {
    return false;
  }

  // Check for multiple module declares
  if (!m_header.moduleName.empty())
  {
    m_logger->error("Multiple 'module' declarations encountered. A script may "
                 "have only one module declaration.");
    return false;
  }
  // Set module in header
  m_header.moduleName = m_lexer.getLexeme();
  return true;
}

bool Assembler::isFunction(const std::string &name)
{
  int32_t id;
  return getFunctionId(name, id);
}

bool Assembler::getFunctionId(const std::string &name, int32_t &id)
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

bool Assembler::isExternFunction(const std::string &name)
{
  int32_t id;
  return getExternFunctionId(name, id);
}

bool Assembler::getExternFunctionId(const std::string &name, int32_t &id)
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

bool Assembler::serializeStrings(std::ostream &stream) const
{
  // Write string count
  uint32_t size = static_cast<uint32_t>(m_strings.size());
  stream.write((char *)&size, sizeof(size));

  // Serialize strings
  for (const auto &str : m_strings)
  {
    // Write size
	uint32_t length = static_cast<uint32_t>(str.length());
    stream.write((char *)&length, sizeof(length));
    // Write data
    stream.write(str.data(), length);
  }
  return stream.good();
}

bool Assembler::serializeExternFunctions(std::ostream &stream) const
{
  // Write extern function count
  uint32_t size = static_cast<uint32_t>(m_externFunctions.size());
  stream.write((char *)&size, sizeof(size));

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

bool Assembler::serializeFunctions(std::ostream &stream) const
{
  // Write function count
  uint32_t size = static_cast<uint32_t>(m_functions.size());
  stream.write((char *)&size, sizeof(size));

  // Serialize functions
  for (const auto &function : m_functions)
  {
    // Write name
    // Write size
	uint32_t length = static_cast<uint32_t>(function.name.length());
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
    size = static_cast<uint32_t>(function.instructions.size());
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

uint32_t Assembler::addString(const std::string &str)
{
  // Add string to table if it does not exist
  uint32_t index = 0;
  for (const auto &entry : m_strings)
  {
    if (entry == str)
    {
      return index;
    }
    ++index;
  }
  // Not found
  m_strings.push_back(str);
  return static_cast<uint32_t>(m_strings.size() - 1);
}
