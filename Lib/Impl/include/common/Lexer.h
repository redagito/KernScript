#pragma once

#include <istream>
#include <string>
#include <unordered_set>

#include <kern/ILogger.h>

#include "common/LexerToken.h"

/**
 * \brief Lexer class.
 */
class Lexer
{
public:
    /**
     * \brief Constructs a lexer.
     * \param logger Receives diagnostics. A null pointer discards them.
     */
    explicit Lexer(kern::ILogger *logger);
    /**
     * \brief Extracts next token/lexeme from stream.
     * \return Number of whitespaces skipped.
     * Writes token and internal lexeme.
     */
    unsigned int lex(std::istream &stream);

    /**
     * \brief Returns the current lexeme, set by getNextToken().
     */
    const std::string &getLexeme() const;
    LexerToken getToken() const;
    /**
     * \brief Returns the 1-based line where the current token starts.
     *
     * Updated by every lex() call, including End and Invalid. Stays
     * unchanged when ignoreNextLex() defers the next lex.
     */
    unsigned getLine() const;
    /**
     * \brief Returns the 1-based column where the current token starts.
     *
     * Updated by every lex() call, including End and Invalid. Stays
     * unchanged when ignoreNextLex() defers the next lex.
     */
    unsigned getColumn() const;

    void addKeyword(const std::string &keyword);
    /**
     * NOTE: removeKeyword and ignoreNextLex below have no callers in Lib/;
     * only the unit tests (Test/UnitTest/source/TestCLexer.cpp) exercise
     * them. They are kept deliberately: removing them would break those
     * tests, which are outside this change, and they round out the
     * keyword/lookahead API beside addKeyword and ignoreNewLine (the
     * latter used by Assembler). Revisit only together with the tests.
     */
    void removeKeyword(const std::string &keyword);

    /**
     * \brief Ignore the next lex command and maintain current the state.
     */
    void ignoreNextLex();
    void ignoreNewLine(bool state);

protected:
    bool isKeyword(const std::string &text) const;

    void lexSingleCharLexeme(std::istream &stream, LexerToken tokenType);
    void lexSingleOrDoubleCharLexeme(std::istream &stream, LexerToken singleToken, char nextPossible,
                                     LexerToken doubleToken);
    void lexSingleOrDoubleCharLexeme(std::istream &stream, LexerToken singleToken, char nextPossible0,
                                     LexerToken doubleToken0, char nextPossible1, LexerToken doubleToken1);

    void lexNumeric(std::istream &stream);
    void lexIdentifierOrKeyword(std::istream &stream);
    void lexString(std::istream &stream);
    void lexComment(std::istream &stream);

    /**
     * \brief Reads one char and advances the line/column cursor.
     *
     * A '\n' moves to the next line; any other char moves one column.
     * A failed extraction leaves the cursor unchanged.
     */
    void takeChar(std::istream &stream, char &c);
    /**
     * \brief Discards one already-peeked char, advancing line/column.
     */
    void skipChar(std::istream &stream, int ch);

private:
    std::unordered_set<std::string> m_keywords;      /**< Registered keywords. */
    std::string m_currentLexeme;                     /**< Currently parsed lexeme. */
    LexerToken m_currentToken = LexerToken::Invalid; /**< Current token. */
    bool m_ignoreNewLine = false;
    bool m_ignoreLex = false;
    unsigned int m_blankSkipped = 0;
    unsigned m_line = 1;               /**< 1-based line of the next char to read. */
    unsigned m_column = 1;             /**< 1-based column of the next char to read. */
    unsigned m_tokenLine = 1;          /**< 1-based line where the current token starts. */
    unsigned m_tokenColumn = 1;        /**< 1-based column where the current token starts. */
    kern::ILogger *m_logger = nullptr; /**< Receives diagnostics. */
};