#include <catch2/catch_test_macros.hpp>

#include "testutil/TestSupport.hpp"

#include "common/Lexer.h"

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

  Lexer lexer(&logger);
  std::stringstream stream("~");

  // An unhandled character is reported through the logger that was passed to
  // this lexer directly. The assembler constructs its own lexer with the same
  // logger, which the assembler tests above verify separately.
  const unsigned int blanks = lexer.lex(stream);

  REQUIRE(blanks == 0);
  REQUIRE_FALSE(logger.empty());
}

TEST_CASE("Lexer takes a null logger without crashing", "[unit][logger]")
{
  Lexer lexer(nullptr);
  std::stringstream stream("42");

  REQUIRE(lexer.lex(stream) == 0);
  REQUIRE(lexer.getToken() == LexerToken::Integer);
  REQUIRE(lexer.getLexeme() == "42");
}

TEST_CASE("Assembler forwards its logger to its internal lexer",
          "[unit][logger]")
{
  testutil::RecordingLogger logger;

  kern::IAssembler *assembler = kern::createAssembler(&logger);
  REQUIRE(assembler != nullptr);

  // The lexer is a member of the assembler, so a character error raised while
  // lexing can only reach this logger if the assembler forwarded the one it
  // was given rather than leaving its member with the null logger.
  std::stringstream source("~");
  std::stringstream byteCode;

  REQUIRE_FALSE(assembler->assemble(source, byteCode));

  // Both the lexer's character error and the assembler's token error have to
  // arrive. The count, not the wording, is asserted.
  REQUIRE(logger.messages().size() >= 2);

  delete assembler;
}

TEST_CASE("A null logger discards messages", "[unit][logger]")
{
  kern::NullLogger logger;
  // No exception and no output.
  logger.error("a message");
  REQUIRE(true);
}
