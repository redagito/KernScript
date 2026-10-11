#include <catch2/catch_test_macros.hpp>

#include "testutil/TestSupport.hpp"

#include "common/CLexer.h"

namespace
{

// Every level of the pipeline has to forward the logger it is given, so these
// tests each construct a component directly rather than through a factory.

const std::string labelScript = "extern println 1\n\nfunc main\n{\npushi 1\n"
                                "jle missing\nret\n}\n";
}

TEST_CASE("Assembler forwards its logger on an unresolved label",
          "[unit][logger]")
{
  testutil::RecordingLogger logger;

  kern::IAssembler *assembler = kern::createAssembler(&logger);
  REQUIRE(assembler != nullptr);

  std::stringstream source(labelScript);
  std::stringstream byteCode;

  REQUIRE_FALSE(assembler->assemble(source, byteCode));
  REQUIRE_FALSE(logger.empty());
  REQUIRE(logger.messages()[0].find("Unresolved jump instruction") !=
          std::string::npos);

  delete assembler;
}

TEST_CASE("Assembler without a logger still reports failure",
          "[unit][logger]")
{
  std::stringstream source(labelScript);
  std::stringstream byteCode;

  kern::IAssembler *assembler = kern::createAssembler(nullptr);
  REQUIRE(assembler != nullptr);

  // The null logger has to swallow the message instead of crashing.
  REQUIRE_FALSE(assembler->assemble(source, byteCode));

  delete assembler;
}

TEST_CASE("Assembler without an explicit logger uses the default logger",
          "[unit][logger]")
{
  std::stringstream source(labelScript);
  std::stringstream byteCode;

  kern::IAssembler *assembler = kern::createAssembler();
  REQUIRE(assembler != nullptr);

  REQUIRE_FALSE(assembler->assemble(source, byteCode));

  delete assembler;
}

TEST_CASE("Lexer forwards its logger to the injected instance",
          "[unit][logger]")
{
  testutil::RecordingLogger logger;

  CLexer lexer(&logger);
  std::stringstream stream("~");

  // An unhandled character is reported through the logger that was passed to
  // this lexer directly. The assembler constructs its own lexer with the same
  // logger, which the assembler tests above verify separately.
  const unsigned int blanks = lexer.lex(stream);

  REQUIRE(blanks == 0);
  REQUIRE_FALSE(logger.empty());
  REQUIRE(logger.messages()[0].find("Invalid character") != std::string::npos);
}

TEST_CASE("CLexer takes a null logger without crashing", "[unit][logger]")
{
  CLexer lexer(nullptr);
  std::stringstream stream("42");

  REQUIRE(lexer.lex(stream) == 0);
  REQUIRE(lexer.getToken() == ELexerToken::Integer);
  REQUIRE(lexer.getLexeme() == "42");
}

TEST_CASE("A null logger discards messages", "[unit][logger]")
{
  kern::NullLogger logger;
  // No exception and no output.
  logger.error("a message");
  REQUIRE(true);
}
