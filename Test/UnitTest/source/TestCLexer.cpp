#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "common/Lexer.h"

TEST_CASE("Lexer scans integer and float literals", "[unit][Lexer]")
{
    std::istringstream stream("42 1.5");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Integer);
    REQUIRE(lexer.getLexeme() == "42");

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Float);
    REQUIRE(lexer.getLexeme() == "1.5");
}

TEST_CASE("Lexer reports a malformed float as invalid", "[unit][Lexer]")
{
    std::istringstream stream("1.");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Invalid);
}

TEST_CASE("Lexer separates identifiers from keywords", "[unit][Lexer]")
{
    std::istringstream stream("name keyword");
    Lexer lexer(nullptr);
    lexer.addKeyword("keyword");

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "name");

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Keyword);
    REQUIRE(lexer.getLexeme() == "keyword");
}

TEST_CASE("Lexer accepts underscores in identifiers", "[unit][Lexer]")
{
    std::istringstream stream("a_b_1");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "a_b_1");
}

TEST_CASE("Lexer accepts a leading underscore in identifiers", "[unit][Lexer]")
{
    std::istringstream stream("_cell0");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "_cell0");
}

TEST_CASE("Lexer removes a keyword again", "[unit][Lexer]")
{
    std::istringstream stream("keyword");
    Lexer lexer(nullptr);
    lexer.addKeyword("keyword");
    lexer.removeKeyword("keyword");

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
}

TEST_CASE("Lexer scans string literals without the quotes", "[unit][Lexer]")
{
    std::istringstream stream("\"hello\"");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::String);
    REQUIRE(lexer.getLexeme() == "hello");
}

TEST_CASE("Lexer keeps backslash escapes inside string literals", "[unit][Lexer]")
{
    std::istringstream stream("\"a\\\"b\"");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::String);
    REQUIRE(lexer.getLexeme() == "a\\\"b");
}

TEST_CASE("Lexer rejects an unterminated string", "[unit][Lexer]")
{
    std::istringstream stream("\"unterminated");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Invalid);
}

TEST_CASE("Lexer skips line comments", "[unit][Lexer]")
{
    std::istringstream stream("// a comment");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Comment);
}

TEST_CASE("Lexer scans single character tokens", "[unit][Lexer]")
{
    std::istringstream stream(". ; , # @ ( ) { } [ ]");
    Lexer lexer(nullptr);

    const LexerToken expected[] = {LexerToken::Dot,
                                   LexerToken::Semicolon,
                                   LexerToken::Comma,
                                   LexerToken::Pound,
                                   LexerToken::At,
                                   LexerToken::OpenParenthesis,
                                   LexerToken::CloseParenthesis,
                                   LexerToken::OpenBrace,
                                   LexerToken::CloseBrace,
                                   LexerToken::OpenBracket,
                                   LexerToken::CloseBracket};

    for (LexerToken token : expected)
    {
        lexer.lex(stream);
        REQUIRE(lexer.getToken() == token);
    }
}

TEST_CASE("Lexer scans compound operator tokens", "[unit][Lexer]")
{
    std::istringstream stream(":: || && == != <= >= ++ -- += -= *= /= %=");
    Lexer lexer(nullptr);

    const LexerToken expected[] = {LexerToken::DoubleColon,    LexerToken::DoublePipe,     LexerToken::DoubleAmpersand,
                                   LexerToken::Equal,          LexerToken::NotEqual,       LexerToken::LessOrEqual,
                                   LexerToken::GreaterOrEqual, LexerToken::Increment,      LexerToken::Decrement,
                                   LexerToken::AddAssign,      LexerToken::SubtractAssign, LexerToken::MultiplyAssign,
                                   LexerToken::DivideAssign,   LexerToken::ModuloAssign};

    for (LexerToken token : expected)
    {
        lexer.lex(stream);
        REQUIRE(lexer.getToken() == token);
    }
}

TEST_CASE("Lexer distinguishes the single character form of operators", "[unit][Lexer]")
{
    std::istringstream stream("| & = ! : < > % / \\ * + -");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Pipe);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Ampersand);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Assign);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::ExclamationMark);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Colon);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::LessThan);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::GreaterThan);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Percent);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Slash);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::BackSlash);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Asterisk);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Plus);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Dash);
}

TEST_CASE("Lexer reports newlines unless they are ignored", "[unit][Lexer]")
{
    std::istringstream stream("\nvar");

    Lexer lexer(nullptr);
    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::NewLine);

    std::istringstream ignoredStream("\nvar");
    Lexer ignoringLexer(nullptr);
    ignoringLexer.ignoreNewLine(true);
    ignoringLexer.lex(ignoredStream);
    REQUIRE(ignoringLexer.getToken() == LexerToken::Identifier);
    REQUIRE(ignoringLexer.getLexeme() == "var");
}

TEST_CASE("Lexer counts skipped blanks", "[unit][Lexer]")
{
    std::istringstream stream("   42");
    Lexer lexer(nullptr);

    REQUIRE(lexer.lex(stream) == 3);
    REQUIRE(lexer.getLexeme() == "42");
}

TEST_CASE("Lexer rejects a 0xFF byte instead of ending", "[unit][Lexer][c8]")
{
    // 0xFF narrows to -1, which equals EOF on platforms with signed char. It
    // must not be mistaken for the end of the stream.
    std::string input(1, static_cast<char>(0xFF));
    std::istringstream stream(input);
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Invalid);
}

TEST_CASE("Lexer rejects a string ending in an escape at end of stream", "[unit][Lexer][c8]")
{
    std::istringstream stream("\"ab\\");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Invalid);
}

TEST_CASE("Lexer rejects a lone quote at end of stream", "[unit][Lexer][c8]")
{
    std::istringstream stream("\"");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Invalid);
}

TEST_CASE("Lexer signals end of stream", "[unit][Lexer]")
{
    std::istringstream stream("");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::End);
}

TEST_CASE("Lexer keeps the current token when the next lex is ignored", "[unit][Lexer]")
{
    std::istringstream stream("name ignored");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);

    lexer.ignoreNextLex();
    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "name");
}

TEST_CASE("Lexer tracks line and column of each token", "[unit][Lexer]")
{
    std::istringstream stream("ab cd\n  ef");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getLexeme() == "ab");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);

    lexer.lex(stream);
    REQUIRE(lexer.getLexeme() == "cd");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 4);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::NewLine);
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 6);

    lexer.lex(stream);
    REQUIRE(lexer.getLexeme() == "ef");
    REQUIRE(lexer.getLine() == 2);
    REQUIRE(lexer.getColumn() == 3);
}

TEST_CASE("Lexer starts the next line at column one", "[unit][Lexer]")
{
    std::istringstream stream("\nvar");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::NewLine);
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);

    lexer.lex(stream);
    REQUIRE(lexer.getLexeme() == "var");
    REQUIRE(lexer.getLine() == 2);
    REQUIRE(lexer.getColumn() == 1);
}

TEST_CASE("Lexer end token carries the end position", "[unit][Lexer]")
{
    std::istringstream stream("a");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::End);
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 2);
}

TEST_CASE("Lexer keeps an at-sign inside identifier bodies", "[unit][Lexer]")
{
    std::istringstream stream("a@b");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "a@b");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);
}

TEST_CASE("Lexer keeps digits after an at-sign in identifiers", "[unit][Lexer]")
{
    std::istringstream stream("f@1");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "f@1");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);
}

TEST_CASE("Lexer keeps doubled at-signs inside identifiers", "[unit][Lexer]")
{
    std::istringstream stream("a@@b");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "a@@b");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);
}

TEST_CASE("Lexer keeps a trailing at-sign in identifiers", "[unit][Lexer]")
{
    std::istringstream stream("a@");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "a@");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);
}

TEST_CASE("Lexer does not merge a leading at-sign into identifiers", "[unit][Lexer]")
{
    std::istringstream stream("@a");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::At);
    REQUIRE(lexer.getLexeme() == "@");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "a");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 2);
}

TEST_CASE("Lexer keeps several at-signs in one identifier", "[unit][Lexer]")
{
    std::istringstream stream("a@b@c");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "a@b@c");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);
}

TEST_CASE("Lexer still scans a lone at-sign as At", "[unit][Lexer]")
{
    std::istringstream stream("@");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::At);
    REQUIRE(lexer.getLexeme() == "@");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::End);
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 2);
}

TEST_CASE("Lexer tracks the column of an at-identifier after blanks", "[unit][Lexer]")
{
    std::istringstream stream("  a@b");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Identifier);
    REQUIRE(lexer.getLexeme() == "a@b");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 3);
}

TEST_CASE("Lexer keeps at-signs verbatim inside strings", "[unit][Lexer]")
{
    std::istringstream stream("\"a@b\"");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::String);
    REQUIRE(lexer.getLexeme() == "a@b");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);
}

TEST_CASE("Lexer keeps at-signs verbatim inside comments", "[unit][Lexer]")
{
    std::istringstream stream("// a@b");
    Lexer lexer(nullptr);

    lexer.lex(stream);
    REQUIRE(lexer.getToken() == LexerToken::Comment);
    REQUIRE(lexer.getLexeme() == "// a@b");
    REQUIRE(lexer.getLine() == 1);
    REQUIRE(lexer.getColumn() == 1);
}
