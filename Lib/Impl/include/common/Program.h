#pragma once
#include "common/Instruction.h"
#include "kern/FunctionSignature.h"
#include "kern/VMConfig.h"
#include <array>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

inline constexpr uint8_t bytecodeMajorVersion = 1;
inline constexpr uint8_t bytecodeMinorVersion = 0;

struct SourceLocation
{
    std::string source;
    uint32_t line = 0;
    uint32_t column = 0;
};
namespace ir
{
struct Local
{
    std::string name;
};
struct String
{
    std::string value;
};
struct FunctionRef
{
    std::string name;
};
struct ExternRef
{
    std::string name;
};
struct LabelRef
{
    std::string name;
};
using Operand = std::variant<int32_t, uint32_t, float, Local, String, FunctionRef, ExternRef, LabelRef>;
struct Operation
{
    Opcode id;
    std::vector<Operand> operands;
    SourceLocation location;
};
struct Label
{
    std::string name;
};
using CodeItem = std::variant<Operation, Label>;
struct Code
{
    std::vector<CodeItem> items;
    void emit(Opcode id, std::initializer_list<Operand> operands = {}, SourceLocation location = {})
    {
        items.emplace_back(Operation{id, operands, std::move(location)});
    }
    void label(std::string name) { items.emplace_back(Label{std::move(name)}); }
    void append(const Code &other) { items.insert(items.end(), other.items.begin(), other.items.end()); }
};
struct LocalDecl
{
    std::string name;
    uint32_t count = 1;
};
struct Extern
{
    std::string name;
    kern::FunctionSignature signature;
    SourceLocation location;
};
struct Function
{
    std::string name;
    kern::FunctionSignature signature;
    std::vector<std::string> params;
    std::vector<LocalDecl> locals;
    Code code;
    SourceLocation location;
};
struct Program
{
    std::string moduleName;
    std::vector<Extern> externs;
    std::vector<Function> functions;
};
} // namespace ir
struct ProgramExtern
{
    std::string name;
    kern::FunctionSignature signature;
};
struct ProgramFunction
{
    std::string name;
    kern::FunctionSignature signature;
    uint32_t frameSize = 0;
    std::vector<Instruction> instructions;
    std::vector<SourceLocation> locations;
};
struct Program
{
    std::string moduleName;
    std::vector<std::string> strings;
    std::vector<ProgramExtern> externs;
    std::vector<ProgramFunction> functions;
};
class VerifiedProgram
{
public:
    const Program &data() const { return m_program; }

private:
    explicit VerifiedProgram(Program program) : m_program(std::move(program)) {}
    Program m_program;
    friend std::optional<VerifiedProgram> verifyProgram(Program, std::string &);
};
bool linkProgram(const ir::Program &, Program &, std::string &error);
std::optional<VerifiedProgram> verifyProgram(Program, std::string &error);
void emitAssembly(const ir::Program &, std::ostream &);
bool serializeProgram(const Program &, std::ostream &);
bool deserializeProgram(std::istream &, Program &, kern::VMConfig, std::string &error);
