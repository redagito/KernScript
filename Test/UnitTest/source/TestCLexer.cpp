#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "common/CLexer.h"

TEST_CASE("Lexer scans integer and float literals", "[unit][CLexer]")
{
  std::istringstream stream("42 1.5");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Integer);
  REQUIRE(lexer.getLexeme() == "42");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Float);
  REQUIRE(lexer.getLexeme() == "1.5");
}

TEST_CASE("Lexer reports a malformed float as invalid", "[unit][CLexer]")
{
  std::istringstream stream("1.");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Invalid);
}

TEST_CASE("Lexer separates identifiers from keywords", "[unit][CLexer]")
{
  std::istringstream stream("name keyword");
  CLexer lexer;
  lexer.addKeyword("keyword");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
  REQUIRE(lexer.getLexeme() == "name");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Keyword);
  REQUIRE(lexer.getLexeme() == "keyword");
}

TEST_CASE("Lexer accepts underscores in identifiers", "[unit][CLexer]")
{
  std::istringstream stream("a_b_1");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
  REQUIRE(lexer.getLexeme() == "a_b_1");
}

TEST_CASE("Lexer removes a keyword again", "[unit][CLexer]")
{
  std::istringstream stream("keyword");
  CLexer lexer;
  lexer.addKeyword("keyword");
  lexer.removeKeyword("keyword");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
}

TEST_CASE("Lexer scans string literals without the quotes", "[unit][CLexer]")
{
  std::istringstream stream("\"hello\"");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::String);
  REQUIRE(lexer.getLexeme() == "hello");
}

TEST_CASE("Lexer keeps backslash escapes inside string literals",
          "[unit][CLexer]")
{
  std::istringstream stream("\"a\\\"b\"");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::String);
  REQUIRE(lexer.getLexeme() == "a\\\"b");
}

TEST_CASE("Lexer rejects an unterminated string", "[unit][CLexer]")
{
  std::istringstream stream("\"unterminated");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Invalid);
}

TEST_CASE("Lexer skips line comments", "[unit][CLexer]")
{
  std::istringstream stream("// a comment");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Comment);
}

TEST_CASE("Lexer scans single character tokens", "[unit][CLexer]")
{
  std::istringstream stream(". ; , # @ ( ) { } [ ]");
  CLexer lexer;

  const ELexerToken expected[] = {ELexerToken::Dot,
                                  ELexerToken::Semicolon,
                                  ELexerToken::Comma,
                                  ELexerToken::Pound,
                                  ELexerToken::At,
                                  ELexerToken::OpenParenthesis,
                                  ELexerToken::CloseParenthesis,
                                  ELexerToken::OpenBrace,
                                  ELexerToken::CloseBrace,
                                  ELexerToken::OpenBracket,
                                  ELexerToken::CloseBracket};

  for (ELexerToken token : expected)
  {
    lexer.lex(stream);
    REQUIRE(lexer.getToken() == token);
  }
}

TEST_CASE("Lexer scans compound operator tokens", "[unit][CLexer]")
{
  std::istringstream stream(":: || && == != <= >= ++ -- += -= *= /= %=");
  CLexer lexer;

  const ELexerToken expected[] = {ELexerToken::DoubleColon,
                                  ELexerToken::DoublePipe,
                                  ELexerToken::DoubleAmpersand,
                                  ELexerToken::Equal,
                                  ELexerToken::NotEqual,
                                  ELexerToken::LessOrEqual,
                                  ELexerToken::GreaterOrEqual,
                                  ELexerToken::Increment,
                                  ELexerToken::Decrement,
                                  ELexerToken::AddAssign,
                                  ELexerToken::SubtractAssign,
                                  ELexerToken::MultiplyAssign,
                                  ELexerToken::DivideAssign,
                                  ELexerToken::ModuloAssign};

  for (ELexerToken token : expected)
  {
    lexer.lex(stream);
    REQUIRE(lexer.getToken() == token);
  }
}

TEST_CASE("Lexer distinguishes the single character form of operators",
          "[unit][CLexer]")
{
  std::istringstream stream("| & = ! : < > % / \\ * + -");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Pipe);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Ampersand);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Assign);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::ExclamationMark);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Colon);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::LessThan);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::GreaterThan);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Percent);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Slash);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::BackSlash);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Asterisk);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Plus);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Dash);
}

TEST_CASE("Lexer reports newlines unless they are ignored", "[unit][CLexer]")
{
  std::istringstream stream("\nvar");

  CLexer lexer;
  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::NewLine);

  std::istringstream ignoredStream("\nvar");
  CLexer ignoringLexer;
  ignoringLexer.ignoreNewLine(true);
  ignoringLexer.lex(ignoredStream);
  REQUIRE(ignoringLexer.getToken() == ELexerToken::Identifier);
  REQUIRE(ignoringLexer.getLexeme() == "var");
}

TEST_CASE("Lexer counts skipped blanks", "[unit][CLexer]")
{
  std::istringstream stream("   42");
  CLexer lexer;

  REQUIRE(lexer.lex(stream) == 3);
  REQUIRE(lexer.getLexeme() == "42");
}

TEST_CASE("Lexer signals end of stream", "[unit][CLexer]")
{
  std::istringstream stream("");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::End);
}

TEST_CASE("Lexer keeps the current token when the next lex is ignored",
          "[unit][CLexer]")
{
  std::istringstream stream("name ignored");
  CLexer lexer;

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);

  lexer.ignoreNextLex();
  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
  REQUIRE(lexer.getLexeme() == "name");
}
