#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "common/Lexer.h"

TEST_CASE("Lexer scans integer and float literals", "[unit][Lexer]")
{
  std::istringstream stream("42 1.5");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Integer);
  REQUIRE(lexer.getLexeme() == "42");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Float);
  REQUIRE(lexer.getLexeme() == "1.5");
}

TEST_CASE("Lexer reports a malformed float as invalid", "[unit][Lexer]")
{
  std::istringstream stream("1.");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Invalid);
}

TEST_CASE("Lexer separates identifiers from keywords", "[unit][Lexer]")
{
  std::istringstream stream("name keyword");
  Lexer lexer(nullptr);
  lexer.addKeyword("keyword");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
  REQUIRE(lexer.getLexeme() == "name");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Keyword);
  REQUIRE(lexer.getLexeme() == "keyword");
}

TEST_CASE("Lexer accepts underscores in identifiers", "[unit][Lexer]")
{
  std::istringstream stream("a_b_1");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
  REQUIRE(lexer.getLexeme() == "a_b_1");
}

TEST_CASE("Lexer removes a keyword again", "[unit][Lexer]")
{
  std::istringstream stream("keyword");
  Lexer lexer(nullptr);
  lexer.addKeyword("keyword");
  lexer.removeKeyword("keyword");

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
}

TEST_CASE("Lexer scans string literals without the quotes", "[unit][Lexer]")
{
  std::istringstream stream("\"hello\"");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::String);
  REQUIRE(lexer.getLexeme() == "hello");
}

TEST_CASE("Lexer keeps backslash escapes inside string literals",
          "[unit][Lexer]")
{
  std::istringstream stream("\"a\\\"b\"");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::String);
  REQUIRE(lexer.getLexeme() == "a\\\"b");
}

TEST_CASE("Lexer rejects an unterminated string", "[unit][Lexer]")
{
  std::istringstream stream("\"unterminated");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Invalid);
}

TEST_CASE("Lexer skips line comments", "[unit][Lexer]")
{
  std::istringstream stream("// a comment");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Comment);
}

TEST_CASE("Lexer scans single character tokens", "[unit][Lexer]")
{
  std::istringstream stream(". ; , # @ ( ) { } [ ]");
  Lexer lexer(nullptr);

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

TEST_CASE("Lexer scans compound operator tokens", "[unit][Lexer]")
{
  std::istringstream stream(":: || && == != <= >= ++ -- += -= *= /= %=");
  Lexer lexer(nullptr);

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
          "[unit][Lexer]")
{
  std::istringstream stream("| & = ! : < > % / \\ * + -");
  Lexer lexer(nullptr);

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

TEST_CASE("Lexer reports newlines unless they are ignored", "[unit][Lexer]")
{
  std::istringstream stream("\nvar");

  Lexer lexer(nullptr);
  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::NewLine);

  std::istringstream ignoredStream("\nvar");
  Lexer ignoringLexer(nullptr);
  ignoringLexer.ignoreNewLine(true);
  ignoringLexer.lex(ignoredStream);
  REQUIRE(ignoringLexer.getToken() == ELexerToken::Identifier);
  REQUIRE(ignoringLexer.getLexeme() == "var");
}

TEST_CASE("Lexer counts skipped blanks", "[unit][Lexer]")
{
  std::istringstream stream("   42");
  Lexer lexer(nullptr);

  REQUIRE(lexer.lex(stream) == 3);
  REQUIRE(lexer.getLexeme() == "42");
}

TEST_CASE("Lexer signals end of stream", "[unit][Lexer]")
{
  std::istringstream stream("");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::End);
}

TEST_CASE("Lexer keeps the current token when the next lex is ignored",
          "[unit][Lexer]")
{
  std::istringstream stream("name ignored");
  Lexer lexer(nullptr);

  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);

  lexer.ignoreNextLex();
  lexer.lex(stream);
  REQUIRE(lexer.getToken() == ELexerToken::Identifier);
  REQUIRE(lexer.getLexeme() == "name");
}
