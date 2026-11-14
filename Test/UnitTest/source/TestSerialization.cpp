#include "common/Program.h"
#include "kern/RuntimeError.h"
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <sstream>
namespace
{
Instruction ins(Opcode op, int32_t a = 0)
{
    Instruction i;
    i.id = op;
    i.args[0] = a;
    return i;
}
Program sample()
{
    Program p;
    p.moduleName = "module";
    p.strings = {std::string("a\0b", 3)};
    p.externs = {{"out", {1, kern::ReturnKind::Void}}};
    p.functions = {
        {"main", {0, kern::ReturnKind::Void}, 1, {ins(Opcode::Pushi, -7), ins(Opcode::Pop), ins(Opcode::Ret)}, {}}};
    return p;
}
std::string encode(const Program &p)
{
    std::stringstream s;
    REQUIRE(serializeProgram(p, s));
    return s.str();
}
} // namespace
TEST_CASE("Program wire format round trips signatures frames strings and instructions", "[unit][serialize]")
{
    const auto original = sample();
    std::stringstream s(encode(original));
    Program result;
    std::string error;
    REQUIRE(deserializeProgram(s, result, {}, error));
    REQUIRE(result.moduleName == "module");
    REQUIRE(result.strings == original.strings);
    REQUIRE(result.externs[0].signature == original.externs[0].signature);
    REQUIRE(result.functions[0].signature == original.functions[0].signature);
    REQUIRE(result.functions[0].frameSize == 1);
    REQUIRE(result.functions[0].instructions[0].args[0] == -7);
    REQUIRE(verifyProgram(std::move(result), error).has_value());
}
TEST_CASE("Wire format uses explicit version and little endian counts", "[unit][serialize]")
{
    const auto data = encode(sample());
    REQUIRE(data.substr(0, 6) == std::string("KERN\1\0", 6));
    REQUIRE(static_cast<unsigned char>(data[6]) == 6);
    REQUIRE(data[7] == 0);
    REQUIRE(data[8] == 0);
    REQUIRE(data[9] == 0);
}
TEST_CASE("Old or future bytecode versions are rejected deterministically", "[unit][serialize]")
{
    for (int version : {0, 2, 255})
    {
        auto data = encode(sample());
        data[4] = static_cast<char>(version);
        std::stringstream s(data);
        Program p;
        std::string error;
        REQUIRE_FALSE(deserializeProgram(s, p, {}, error));
        REQUIRE(error.find("Unsupported bytecode") != std::string::npos);
    }
}
TEST_CASE("Every truncation boundary is rejected", "[unit][serialize]")
{
    const auto data = encode(sample());
    for (size_t i = 0; i < data.size(); ++i)
    {
        CAPTURE(i);
        std::stringstream s(data.substr(0, i));
        Program p;
        std::string error;
        REQUIRE_FALSE(deserializeProgram(s, p, {}, error));
    }
}
TEST_CASE("Numeric operand bit patterns survive the portable codec", "[unit][serialize]")
{
    for (auto bits : {0u, 1u, 0x80000000u, 0xffffffffu, 0x7fc12345u, 0x3fc00000u})
    {
        auto p = sample();
        p.functions[0].instructions[0] = ins(Opcode::Pushf, std::bit_cast<int32_t>(bits));
        std::stringstream s(encode(p));
        Program decoded;
        std::string error;
        REQUIRE(deserializeProgram(s, decoded, {}, error));
        REQUIRE(std::bit_cast<uint32_t>(decoded.functions[0].instructions[0].args[0]) == bits);
    }
}
TEST_CASE("Invalid signature kinds and frame layouts cannot become executable", "[unit][serialize]")
{
    for (int mode = 0; mode < 3; ++mode)
    {
        auto p = sample();
        if (mode == 0)
            p.functions[0].signature.returnKind = static_cast<kern::ReturnKind>(3);
        if (mode == 1)
            p.externs[0].signature.returnKind = static_cast<kern::ReturnKind>(3);
        if (mode == 2)
            p.functions[0].signature.argumentCount = 2;
        std::string error;
        REQUIRE_FALSE(verifyProgram(std::move(p), error));
    }
}
TEST_CASE("Duplicate and empty symbols are rejected by verification", "[unit][serialize]")
{
    for (int mode = 0; mode < 4; ++mode)
    {
        auto p = sample();
        if (mode == 0)
            p.functions.push_back(p.functions[0]);
        if (mode == 1)
            p.externs.push_back(p.externs[0]);
        if (mode == 2)
            p.functions[0].name = "";
        if (mode == 3)
            p.functions[0].name = p.externs[0].name;
        std::string error;
        REQUIRE_FALSE(verifyProgram(std::move(p), error));
    }
}
TEST_CASE("Linker assigns parameter slots before locals regardless of declaration placement", "[unit][ir]")
{
    ir::Program p;
    ir::Function f;
    f.name = "identity";
    f.signature = {1, kern::ReturnKind::Value};
    f.params = {"x"};
    f.locals = {{"before", 3}};
    f.code.emit(Opcode::Pushv, {ir::Local{"x"}});
    f.code.emit(Opcode::Ret);
    p.functions.push_back(f);
    Program linked;
    std::string error;
    REQUIRE(linkProgram(p, linked, error));
    REQUIRE(linked.functions[0].frameSize == 4);
    REQUIRE(linked.functions[0].instructions[0].args[0] == 0);
    REQUIRE(verifyProgram(std::move(linked), error));
}
TEST_CASE("Typed IR refuses a mismatched operand kind", "[unit][ir]")
{
    ir::Program p;
    ir::Function f;
    f.name = "main";
    f.code.emit(Opcode::Pushf, {int32_t(42)});
    f.code.emit(Opcode::Pop);
    p.functions.push_back(f);
    Program result;
    std::string error;
    REQUIRE_FALSE(linkProgram(p, result, error));
}
TEST_CASE("Unknown symbols and duplicate labels fail linking", "[unit][ir]")
{
    for (int mode = 0; mode < 4; ++mode)
    {
        ir::Program p;
        ir::Function f;
        f.name = "main";
        if (mode == 0)
            f.code.emit(Opcode::Pushv, {ir::Local{"missing"}});
        if (mode == 1)
            f.code.emit(Opcode::Call, {ir::FunctionRef{"missing"}});
        if (mode == 2)
            f.code.emit(Opcode::Jmp, {ir::LabelRef{"missing"}});
        if (mode == 3)
        {
            f.code.label("same");
            f.code.label("same");
        }
        p.functions.push_back(f);
        Program result;
        std::string error;
        REQUIRE_FALSE(linkProgram(p, result, error));
    }
}
TEST_CASE("Return conveniences normalize into canonical operations", "[unit][ir]")
{
    ir::Program p;
    ir::Function f;
    f.name = "value";
    f.signature.returnKind = kern::ReturnKind::Value;
    f.code.emit(Opcode::Reti, {int32_t(42)});
    p.functions.push_back(f);
    Program linked;
    std::string error;
    REQUIRE(linkProgram(p, linked, error));
    REQUIRE(linked.functions[0].instructions.size() == 2);
    REQUIRE(linked.functions[0].instructions[0].id == Opcode::Pushi);
    REQUIRE(linked.functions[0].instructions[1].id == Opcode::Ret);
    REQUIRE(verifyProgram(std::move(linked), error));
}
TEST_CASE("Void trailing labels resolve to a real return instruction", "[unit][ir]")
{
    ir::Program p;
    ir::Function f;
    f.name = "main";
    f.code.emit(Opcode::Jmp, {ir::LabelRef{"end"}});
    f.code.label("end");
    p.functions.push_back(f);
    Program linked;
    std::string error;
    REQUIRE(linkProgram(p, linked, error));
    REQUIRE(linked.functions[0].instructions[1].id == Opcode::Ret);
    REQUIRE(verifyProgram(std::move(linked), error));
}
TEST_CASE("Value functions cannot jump to a nonexistent implicit return", "[unit][ir]")
{
    ir::Program p;
    ir::Function f;
    f.name = "bad";
    f.signature.returnKind = kern::ReturnKind::Value;
    f.code.emit(Opcode::Jmp, {ir::LabelRef{"end"}});
    f.code.label("end");
    p.functions.push_back(f);
    Program linked;
    std::string error;
    REQUIRE(linkProgram(p, linked, error));
    REQUIRE_FALSE(verifyProgram(std::move(linked), error));
}
TEST_CASE("Opcode names cover the complete instruction enum", "[unit][ir]")
{
    for (size_t i = 0; i < std::size(instructions); ++i)
    {
        REQUIRE_FALSE(instructions[i].mnemonic.empty());
        REQUIRE(instructions[i].argCount <= 4);
    }
    Instruction unknown;
    unknown.id = static_cast<Opcode>(65535);
    REQUIRE(toString(unknown) == "<invalid>");
}
