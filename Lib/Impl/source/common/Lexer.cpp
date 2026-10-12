#include "common/Lexer.h"

#include <cassert>
#include <cctype>
#include <cstdio>

#include "logging/Logger.h"

Lexer::Lexer(kern::ILogger *logger) : m_logger(logger != nullptr ? logger : nullLogger())
{
    // Empty
}

unsigned int Lexer::lex(std::istream &stream)
{
    if (m_ignoreLex)
    {
        m_ignoreLex = false;
        return m_blankSkipped;
    }

    // Reset lexeme and token
    m_currentLexeme.clear();
    m_currentToken = LexerToken::Invalid;

    // peek() returns int so that EOF (-1) stays distinct. Narrowing to char
    // first would turn a 0xFF byte into -1 as well, and handing a negative
    // value other than EOF to the ctype functions below is undefined.
    int next = stream.peek();

    // For counting whitespaces
    m_blankSkipped = 0;
    // Skip blanks
    while (next == ' ' || next == '\t' || next == '\r' || (m_ignoreNewLine && next == '\n'))
    {
        // Skip blank
        skipChar(stream, next);
        if (next != '\n' && next != '\r')
        {
            ++m_blankSkipped;
        }
        // Peek next
        next = stream.peek();
    }

    // The token starts after the skipped blanks.
    m_tokenLine = m_line;
    m_tokenColumn = m_column;

    // End of stream is decided on the int, before any narrowing.
    if (next == EOF)
    {
        m_currentToken = LexerToken::End;
        return m_blankSkipped;
    }

    // The ctype functions take EOF or a value representable as unsigned char.
    const unsigned char narrow = static_cast<unsigned char>(next);

    // Check for numeric values
    if (std::isdigit(narrow))
    {
        // The next lexeme is a numeric type, either int or float
        lexNumeric(stream);
    }
    else if (std::isalpha(narrow) || next == '_')
    {
        // Either identifier or keyword; a leading underscore is legal,
        // matching the documented [A-Za-z_][A-Za-z0-9_]* rule.
        lexIdentifierOrKeyword(stream);
    }
    else
    {
        switch (next)
        {
        case '.':
            lexSingleCharLexeme(stream, LexerToken::Dot);
            break;
        case ':':
            // Either ':' or '::'
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Colon, ':', LexerToken::DoubleColon);
            break;
        case ';':
            lexSingleCharLexeme(stream, LexerToken::Semicolon);
            break;
        case ',':
            lexSingleCharLexeme(stream, LexerToken::Comma);
            break;
        case '#':
            lexSingleCharLexeme(stream, LexerToken::Pound);
            break;
        case '@':
            lexSingleCharLexeme(stream, LexerToken::At);
            break;
        case '(':
            lexSingleCharLexeme(stream, LexerToken::OpenParenthesis);
            break;
        case ')':
            lexSingleCharLexeme(stream, LexerToken::CloseParenthesis);
            break;
        case '{':
            lexSingleCharLexeme(stream, LexerToken::OpenBrace);
            break;
        case '}':
            lexSingleCharLexeme(stream, LexerToken::CloseBrace);
            break;
        case '[':
            lexSingleCharLexeme(stream, LexerToken::OpenBracket);
            break;
        case ']':
            lexSingleCharLexeme(stream, LexerToken::CloseBracket);
            break;
        case '!':
            // Either just '!' or '!='
            lexSingleOrDoubleCharLexeme(stream, LexerToken::ExclamationMark, '=', LexerToken::NotEqual);
            break;
        case '?':
            lexSingleCharLexeme(stream, LexerToken::QuestionMark);
            break;
        case '&':
            // Either '&' or '&&'
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Ampersand, '&', LexerToken::DoubleAmpersand);
            break;
        case '|':
            // Either '|' or '||'
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Pipe, '|', LexerToken::DoublePipe);
            break;
        case '<':
            // Either '<' or '<='
            lexSingleOrDoubleCharLexeme(stream, LexerToken::LessThan, '=', LexerToken::LessOrEqual);
            break;
        case '>':
            // Either '>' or '>='
            lexSingleOrDoubleCharLexeme(stream, LexerToken::GreaterThan, '=', LexerToken::GreaterOrEqual);
            break;
        case '%':
            // Either '%' or '%='
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Percent, '=', LexerToken::ModuloAssign);
            break;
        case '/':
            // Might be comment "//" or divide and assign "/=" or slash
            // Multiline comments "/* ... */" currently not supported
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Slash, '=', LexerToken::DivideAssign, '/',
                                        LexerToken::Comment);
            if (m_currentToken == LexerToken::Comment)
            {
                lexComment(stream);
            }
            break;
        case '\\':
            lexSingleCharLexeme(stream, LexerToken::BackSlash);
            break;
        case '=':
            // Either '=' or '=='
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Assign, '=', LexerToken::Equal);
            break;
        case '*':
            // Either '*' or '*='
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Asterisk, '=', LexerToken::MultiplyAssign);
            break;
        case '+':
            // Either '+' or '+=' or '++'
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Plus, '=', LexerToken::AddAssign, '+',
                                        LexerToken::Increment);
            break;
        case '-':
            // Either '-' or '-=' or '--'
            lexSingleOrDoubleCharLexeme(stream, LexerToken::Dash, '=', LexerToken::SubtractAssign, '-',
                                        LexerToken::Decrement);
            break;
        case '"':
            lexString(stream);
            break;
        case '\n':
            lexSingleCharLexeme(stream, LexerToken::NewLine);
            break;
        default:
            // Error. EOF is handled before the switch, so everything landing here
            // is a genuinely unexpected byte, logged as a character.
            m_logger->error("{}:{}:{}: error: Invalid character '{}' encountered.", m_sourceName, m_tokenLine,
                            m_tokenColumn, static_cast<char>(next));
            break;
        }
    }
    return m_blankSkipped;
}

const std::string &Lexer::getLexeme() const { return m_currentLexeme; }

LexerToken Lexer::getToken() const { return m_currentToken; }

unsigned Lexer::getLine() const { return m_tokenLine; }

unsigned Lexer::getColumn() const { return m_tokenColumn; }

void Lexer::addKeyword(const std::string &keyword) { m_keywords.insert(keyword); }

void Lexer::removeKeyword(const std::string &keyword) { m_keywords.erase(keyword); }

void Lexer::ignoreNextLex() { m_ignoreLex = true; }

void Lexer::ignoreNewLine(bool state) { m_ignoreNewLine = state; }

void Lexer::setSourceName(const std::string &sourceName) { m_sourceName = sourceName; }

bool Lexer::isKeyword(const std::string &text) const { return m_keywords.find(text) != m_keywords.end(); }

void Lexer::lexSingleCharLexeme(std::istream &stream, LexerToken tokenType)
{
    // Retrieve next char
    char c;
    takeChar(stream, c);
    // Store
    m_currentLexeme.push_back(c);
    // Set token
    m_currentToken = tokenType;
}

void Lexer::lexSingleOrDoubleCharLexeme(std::istream &stream, LexerToken singleToken, char nextPossible,
                                        LexerToken doubleToken)
{
    // Retrieve char and store
    char c;
    takeChar(stream, c);
    m_currentLexeme.push_back(c);

    // Look ahead and check if lexeme is double char sequence
    if (stream.peek() == nextPossible)
    {
        // Add next char to lexeme
        takeChar(stream, c);
        m_currentLexeme.push_back(c);
        // Set token to double token type
        m_currentToken = doubleToken;
    }
    else
    {
        // Set token to single token sequence
        m_currentToken = singleToken;
    }
}

void Lexer::lexSingleOrDoubleCharLexeme(std::istream &stream, LexerToken singleToken, char nextPossible0,
                                        LexerToken doubleToken0, char nextPossible1, LexerToken doubleToken1)
{
    // Retrieve char and store
    char c;
    takeChar(stream, c);
    m_currentLexeme.push_back(c);

    // Look ahead and check if lexeme is double char sequence
    if (stream.peek() == nextPossible0)
    {
        // Add next char to lexeme
        takeChar(stream, c);
        m_currentLexeme.push_back(c);
        // Set token to double token type
        m_currentToken = doubleToken0;
    }
    else if (stream.peek() == nextPossible1)
    {
        // Add next char to lexeme
        takeChar(stream, c);
        m_currentLexeme.push_back(c);
        // Set token to double token type
        m_currentToken = doubleToken1;
    }
    else
    {
        // Set token to single token sequence
        m_currentToken = singleToken;
    }
}

void Lexer::lexNumeric(std::istream &stream)
{
    // Default is integer until proven otherwise
    m_currentToken = LexerToken::Integer;

    // Loop through chars until non-digit is found
    while (std::isdigit(stream.peek()))
    {
        char c;
        takeChar(stream, c);
        m_currentLexeme.push_back(c);
    }

    // Check for dot
    if (stream.peek() != '.')
    {
        // Next token not a dot means end of integer, otherwise its a float
        return;
    }

    // After dot, at least one additional digit is required
    // Set token to invalid until at least one digit is read
    m_currentToken = LexerToken::Invalid;

    char c;
    takeChar(stream, c);
    m_currentLexeme.push_back(c);

    // Read remaining digits
    while (std::isdigit(stream.peek()))
    {
        // Valid float found
        m_currentToken = LexerToken::Float;
        char c;
        takeChar(stream, c);
        m_currentLexeme.push_back(c);
    }
}

void Lexer::lexIdentifierOrKeyword(std::istream &stream)
{
    // Already checked for a leading alphabetic character or underscore
    // Valid characters in a, identifier are alphabetic, numeric and underscore
    while (std::isalnum(stream.peek()) || stream.peek() == '_')
    {
        // Build lexeme
        char c;
        takeChar(stream, c);
        m_currentLexeme.push_back(c);
    }

    // Default to identifier
    m_currentToken = LexerToken::Identifier;
    // Check if lexeme is keyword
    if (isKeyword(m_currentLexeme))
    {
        m_currentToken = LexerToken::Keyword;
    }
}

void Lexer::lexString(std::istream &stream)
{
    // Throw away first quote
    assert(stream.peek() == '"');
    char c;
    takeChar(stream, c);

    bool escaped = false;
    while (!(stream.peek() == '"' && !escaped))
    {
        takeChar(stream, c);
        // A failed extraction (EOF or stream error) leaves c holding stale
        // content. The string is unterminated, so stop instead of processing it.
        if (stream.fail())
        {
            m_currentToken = LexerToken::Invalid;
            return;
        }
        if (!escaped)
        {
            if (c == '\\')
            {
                escaped = true;
            }
        }
        else
        {
            escaped = false;
        }

        // Check for line break (which is invalid in strings) or unexpected EOF
        // stream signal
        if (stream.peek() == '\n' || stream.peek() == EOF)
        {
            m_currentToken = LexerToken::Invalid;
            return;
        }
        m_currentLexeme.push_back(c);
    }
    // Throw away last quote
    assert(stream.peek() == '"');
    takeChar(stream, c);
    m_currentToken = LexerToken::String;
}

void Lexer::lexComment(std::istream &stream)
{
    // The '//' part has already been written, add remaining line
    // without newline character to lexeme
    // Check for newline or end of stream
    while (!(stream.peek() == '\n' || stream.peek() == EOF))
    {
        char c;
        takeChar(stream, c);
        m_currentLexeme.push_back(c);
    }
}

void Lexer::takeChar(std::istream &stream, char &c)
{
    stream.get(c);
    if (stream.fail())
    {
        return;
    }
    if (c == '\n')
    {
        ++m_line;
        m_column = 1;
    }
    else
    {
        ++m_column;
    }
}

void Lexer::skipChar(std::istream &stream, int ch)
{
    stream.get();
    if (ch == '\n')
    {
        ++m_line;
        m_column = 1;
    }
    else
    {
        ++m_column;
    }
}
