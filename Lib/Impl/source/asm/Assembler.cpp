#include "asm/Assembler.h"
#include "common/Lexer.h"
#include "common/Program.h"
#include "common/FloatLiteral.h"
#include "logging/Logger.h"
#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace
{
class AssemblyParser
{
    Lexer lexer;
    std::istream &input;
    ir::Program program;
    void next()
    {
        do
        {
            lexer.lex(input);
        } while (lexer.getToken() == LexerToken::Comment);
    }
    [[noreturn]] void fail(const std::string &message)
    {
        throw std::runtime_error("Assembly " + std::to_string(lexer.getLine()) + ":" +
                                 std::to_string(lexer.getColumn()) + ": " + message);
    }
    void expect(LexerToken token, const char *what)
    {
        if (lexer.getToken() != token)
            fail(std::string("Expected ") + what + ".");
        next();
    }
    std::string name()
    {
        if (lexer.getToken() != LexerToken::Identifier && lexer.getToken() != LexerToken::String)
            fail("Expected a name.");
        std::string result = lexer.getLexeme();
        if (lexer.getToken() == LexerToken::String)
        {
            std::string decoded;
            for (size_t i = 0; i < result.size(); ++i)
            {
                if (result[i] == '\\' && i + 1 < result.size() && (result[i + 1] == '\\' || result[i + 1] == '"'))
                    ++i;
                decoded.push_back(result[i]);
            }
            result = std::move(decoded);
        }
        if (result.empty())
            fail("Names must not be empty.");
        next();
        return result;
    }
    kern::ReturnKind kind()
    {
        expect(LexerToken::Dash, "'->'");
        expect(LexerToken::GreaterThan, "'->'");
        const auto text = name();
        if (text == "value")
            return kern::ReturnKind::Value;
        if (text == "void")
            return kern::ReturnKind::Void;
        fail("Expected 'value' or 'void'.");
    }
    template <class T> T integer()
    {
        std::string text;
        if (lexer.getToken() == LexerToken::Dash)
        {
            text = "-";
            next();
        }
        if (lexer.getToken() != LexerToken::Integer)
            fail("Expected integer literal.");
        text += lexer.getLexeme();
        next();
        T result{};
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), result);
        if (ec != std::errc() || end != text.data() + text.size())
            fail("Integer literal out of range.");
        return result;
    }
    float floating()
    {
        std::string text;
        if (lexer.getToken() == LexerToken::Dash)
        {
            text = "-";
            next();
        }
        if (lexer.getToken() != LexerToken::Integer && lexer.getToken() != LexerToken::Float)
            fail("Expected float literal.");
        text += lexer.getLexeme();
        next();
        float result = 0;
        if (!parseFloatLiteral(text, result))
            fail("Float literal out of range.");
        return result;
    }
    ir::String string()
    {
        if (lexer.getToken() != LexerToken::String)
            fail("Expected string literal.");
        auto text = lexer.getLexeme();
        next();
        return {text};
    }
    ir::Operation operation(const std::string &mnemonic, SourceLocation location)
    {
        size_t raw = 0;
        for (; raw < std::size(instructions); ++raw)
            if (instructions[raw].mnemonic == mnemonic)
                break;
        if (raw == std::size(instructions))
            fail("Unknown instruction '" + mnemonic + "'.");
        ir::Operation op{static_cast<Opcode>(raw), {}, location};
        for (unsigned i = 0; i < instructions[raw].argCount; ++i)
        {
            if (i)
                expect(LexerToken::Comma, "','");
            switch (instructions[raw].operands[i])
            {
            case OperandKind::Local:
                op.operands.emplace_back(ir::Local{name()});
                break;
            case OperandKind::String:
                op.operands.emplace_back(string());
                break;
            case OperandKind::Integer:
                op.operands.emplace_back(integer<int32_t>());
                break;
            case OperandKind::Unsigned:
                op.operands.emplace_back(integer<uint32_t>());
                break;
            case OperandKind::Float:
                op.operands.emplace_back(floating());
                break;
            case OperandKind::Function:
                op.operands.emplace_back(ir::FunctionRef{name()});
                break;
            case OperandKind::Extern:
                op.operands.emplace_back(ir::ExternRef{name()});
                break;
            case OperandKind::Label:
                op.operands.emplace_back(ir::LabelRef{name()});
                break;
            default:
                fail("Invalid operand schema.");
            }
        }
        return op;
    }

public:
    AssemblyParser(std::istream &s, kern::ILogger *logger) : lexer(logger), input(s)
    {
        lexer.ignoreNewLine(true);
        next();
    }
    ir::Program parse()
    {
        bool module = false;
        while (lexer.getToken() != LexerToken::End)
        {
            const auto word = name();
            if (word == "module")
            {
                if (module)
                    fail("Duplicate module.");
                program.moduleName = name();
                module = true;
            }
            else if (word == "extern")
            {
                ir::Extern e;
                e.name = name();
                e.signature.argumentCount = integer<uint32_t>();
                e.signature.returnKind = kind();
                program.externs.push_back(std::move(e));
            }
            else if (word == "func")
            {
                ir::Function f;
                f.name = name();
                f.signature.returnKind = kind();
                expect(LexerToken::OpenBrace, "'{'");
                while (lexer.getToken() != LexerToken::CloseBrace)
                {
                    if (lexer.getToken() == LexerToken::End)
                        fail("Unterminated function.");
                    SourceLocation location{"<assembly>", lexer.getLine(), lexer.getColumn()};
                    const auto mnemonic = name();
                    if (mnemonic == "param")
                        f.params.push_back(name());
                    else if (mnemonic == "var")
                        f.locals.push_back({name(), 1});
                    else if (mnemonic == "vara")
                    {
                        auto local = name();
                        auto count = integer<uint32_t>();
                        f.locals.push_back({local, count});
                    }
                    else if (mnemonic == "label")
                        f.code.label(name());
                    else
                        f.code.items.emplace_back(operation(mnemonic, location));
                }
                next();
                f.signature.argumentCount = static_cast<uint32_t>(f.params.size());
                program.functions.push_back(std::move(f));
            }
            else
                fail("Expected module, extern, or func declaration.");
        }
        return std::move(program);
    }
};
} // namespace
Assembler::Assembler(kern::ILogger *logger) : m_logger(logger ? logger : nullLogger()) {}
bool Assembler::assemble(std::istream &source, std::ostream &bytes)
{
    try
    {
        auto ir = AssemblyParser(source, m_logger).parse();
        Program p;
        std::string error;
        if (!linkProgram(ir, p, error))
        {
            m_logger->error("{}", error);
            return false;
        }
        auto verified = verifyProgram(std::move(p), error);
        if (!verified)
        {
            m_logger->error("{}", error);
            return false;
        }
        return serializeProgram(verified->data(), bytes);
    }
    catch (const std::exception &e)
    {
        m_logger->error("{}", e.what());
        return false;
    }
}
