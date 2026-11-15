#include "common/Program.h"
#include "testutil/TestSupport.hpp"
#include "vm/VirtualMachine.h"
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <random>
#include <sstream>

namespace
{
Instruction instruction(Opcode op, int32_t a = 0, int32_t b = 0)
{
    Instruction i;
    i.id = op;
    i.args[0] = a;
    i.args[1] = b;
    return i;
}
Program program(std::vector<Instruction> code, kern::ReturnKind kind = kern::ReturnKind::Void, uint32_t slots = 0,
                uint32_t args = 0)
{
    Program p;
    p.functions.push_back({"main", {args, kind}, slots, std::move(code), {}});
    return p;
}
std::stringstream bytes(const Program &p)
{
    std::stringstream s;
    REQUIRE(serializeProgram(p, s));
    return s;
}
std::stringstream assembled(const std::string &body)
{
    std::stringstream s;
    REQUIRE(testutil::assemble("extern println 1 -> void\nfunc main -> void {\n" + body + "\n}\n", s));
    return s;
}
void rejected(const Program &p)
{
    auto s = bytes(p);
    VirtualMachine vm(nullptr);
    REQUIRE_FALSE(vm.load(s));
    REQUIRE_FALSE(vm.callFunction("main"));
}
struct Host : kern::IExternFunction
{
    kern::FunctionSignature signature;
    std::function<bool(kern::IExternCall &)> action;
    Host(kern::FunctionSignature sig, std::function<bool(kern::IExternCall &)> fn)
        : signature(sig), action(std::move(fn))
    {
    }
    kern::FunctionSignature getSignature() const override { return signature; }
    bool call(kern::IExternCall &call) override { return action(call); }
};
std::stringstream hostProgram(kern::FunctionSignature signature, std::vector<Instruction> code)
{
    auto p = program(std::move(code));
    p.externs.push_back({"host", signature});
    return bytes(p);
}
} // namespace

TEST_CASE("Verifier rejects every instruction that underflows its operand region", "[integration][vm][verifier]")
{
    for (auto op : {Opcode::Pop, Opcode::Popv, Opcode::Add, Opcode::Sub, Opcode::Mul, Opcode::Div, Opcode::Inc,
                    Opcode::Dec, Opcode::And, Opcode::Or, Opcode::Xor, Opcode::Not, Opcode::Je, Opcode::Jne, Opcode::Jg,
                    Opcode::Jge, Opcode::Jl, Opcode::Jle, Opcode::Neg})
    {
        CAPTURE(static_cast<int>(op));
        rejected(program({instruction(op), instruction(Opcode::Ret)}, kern::ReturnKind::Void, 1));
    }
}
TEST_CASE("Verifier validates references even in unreachable instructions", "[integration][vm][verifier]")
{
    for (auto op : {Opcode::Movv, Opcode::Movi, Opcode::Movf, Opcode::Movs, Opcode::Pushv, Opcode::Popv, Opcode::Pushs,
                    Opcode::Call, Opcode::Calle, Opcode::Jmp})
    {
        CAPTURE(static_cast<int>(op));
        rejected(program({instruction(Opcode::Exit), instruction(op, -1), instruction(Opcode::Ret)}));
    }
    std::string error;
    REQUIRE_FALSE(verifyProgram(program({instruction(static_cast<Opcode>(65535))}), error));
}
TEST_CASE("Value return aliases cannot appear in executable bytecode", "[integration][vm][verifier]")
{
    for (auto op : {Opcode::Retv, Opcode::Reti, Opcode::Retf, Opcode::Rets})
    {
        CAPTURE(static_cast<int>(op));
        rejected(program({instruction(op)}));
    }
}
TEST_CASE("Verifier rejects inconsistent branch joins and growing loops", "[integration][vm][verifier]")
{
    rejected(program({instruction(Opcode::Pushi, 1), instruction(Opcode::Pushi, 1), instruction(Opcode::Je, 4),
                      instruction(Opcode::Pushi, 3), instruction(Opcode::Ret)}));
    rejected(program({instruction(Opcode::Pushi, 1), instruction(Opcode::Jmp, 0)}));
}
TEST_CASE("Verifier enforces result counts and explicit termination", "[integration][vm][verifier]")
{
    rejected(program({instruction(Opcode::Pushi, 1), instruction(Opcode::Ret)}));
    rejected(program({instruction(Opcode::Ret)}, kern::ReturnKind::Value));
    rejected(program({instruction(Opcode::Pushi, 1)}, kern::ReturnKind::Value));
    rejected(program({instruction(Opcode::Nop)}));
    rejected(program({}));
    rejected(program({instruction(Opcode::Ret)}, kern::ReturnKind::Void, 0, 1));
}
TEST_CASE("Missing script arguments cannot consume caller locals", "[integration][vm][verifier]")
{
    auto p = program({instruction(Opcode::Movi, 0, 7), instruction(Opcode::Call, 1), instruction(Opcode::Ret)},
                     kern::ReturnKind::Void, 1);
    p.functions.push_back({"callee", {1, kern::ReturnKind::Void}, 1, {instruction(Opcode::Ret)}, {}});
    rejected(p);
}
TEST_CASE("Locals and operands remain isolated through nested script calls", "[integration][vm]")
{
    const std::string text =
        "extern println 1 -> void\nfunc addone -> value { var scratch param x pushv x pushi 1 add ret }\n"
        "func main -> void {var keep movi keep, 99 pushi 10 pushi 5 call addone add calle println pushv keep calle "
        "println ret}";
    testutil::StringRecorder out;
    REQUIRE(testutil::runScript(text, "main", out));
    REQUIRE(out.text() == "16\n99\n");
}
TEST_CASE("Recursion preserves each frame and obeys the depth budget", "[integration][vm]")
{
    const std::string text = "func recurse -> void { call recurse ret } func clean -> void {ret}";
    std::stringstream s;
    REQUIRE(testutil::assemble(text, s));
    kern::VMConfig config;
    config.maxCallDepth = 5;
    VirtualMachine vm(nullptr, config);
    REQUIRE(vm.load(s));
    REQUIRE_THROWS_AS(vm.callFunction("recurse"), kern::RuntimeError);
    REQUIRE(vm.callFunction("clean"));
}
TEST_CASE("Entry functions must be zero argument and void", "[integration][vm]")
{
    for (auto p : {program({instruction(Opcode::Pushi, 1), instruction(Opcode::Ret)}, kern::ReturnKind::Value),
                   program({instruction(Opcode::Ret)}, kern::ReturnKind::Void, 1, 1)})
    {
        auto s = bytes(p);
        VirtualMachine vm(nullptr);
        REQUIRE(vm.load(s));
        REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
    }
}
TEST_CASE("Unknown entry names return false", "[integration][vm]")
{
    VirtualMachine vm(nullptr);
    REQUIRE_FALSE(vm.callFunction("missing"));
    auto s = assembled("ret");
    REQUIRE(vm.load(s));
    REQUIRE_FALSE(vm.callFunction("missing"));
    REQUIRE(vm.callFunction("main"));
}
TEST_CASE("Host arguments are indexed and preserve pending operands", "[integration][vm][host]")
{
    auto s = hostProgram({2, kern::ReturnKind::Value},
                         {instruction(Opcode::Pushi, 100), instruction(Opcode::Pushi, 9), instruction(Opcode::Pushi, 4),
                          instruction(Opcode::Calle, 0), instruction(Opcode::Add), instruction(Opcode::Pop),
                          instruction(Opcode::Ret)});
    int count = 0;
    VirtualMachine vm(nullptr);
    vm.addFunction("host", new Host({2, kern::ReturnKind::Value},
                                    [&](kern::IExternCall &call)
                                    {
                                        int a = 0, b = 0, again = 0;
                                        REQUIRE(call.getArgument(0, a));
                                        REQUIRE(call.getArgument(1, b));
                                        REQUIRE(call.getArgument(0, again));
                                        REQUIRE(a == 9);
                                        REQUIRE(b == 4);
                                        REQUIRE(again == a);
                                        ++count;
                                        call.setReturnValue(a - b);
                                        return true;
                                    }));
    REQUIRE(vm.load(s));
    REQUIRE(vm.callFunction("main"));
    REQUIRE(count == 1);
}
TEST_CASE("Host context bounds cannot reach locals or previous operands", "[integration][vm][host]")
{
    auto s = hostProgram({0, kern::ReturnKind::Void}, {instruction(Opcode::Pushi, 99), instruction(Opcode::Calle),
                                                       instruction(Opcode::Pop), instruction(Opcode::Ret)});
    VirtualMachine vm(nullptr);
    vm.addFunction("host", new Host({0, kern::ReturnKind::Void},
                                    [](kern::IExternCall &call)
                                    {
                                        int v = 0;
                                        REQUIRE_THROWS_AS(call.getArgument(0, v), kern::RuntimeError);
                                        return true;
                                    }));
    REQUIRE(vm.load(s));
    REQUIRE(vm.callFunction("main"));
}
TEST_CASE("Host conversions preserve numeric boundaries and strings", "[integration][vm][host]")
{
    auto p = program({instruction(Opcode::Pushu, std::bit_cast<int32_t>(3000000000u)), instruction(Opcode::Calle),
                      instruction(Opcode::Ret)});
    p.externs.push_back({"host", {1, kern::ReturnKind::Void}});
    auto s = bytes(p);
    VirtualMachine vm(nullptr);
    vm.addFunction("host", new Host({1, kern::ReturnKind::Void},
                                    [](kern::IExternCall &call)
                                    {
                                        unsigned int u = 0;
                                        double d = 0;
                                        bool b = false;
                                        std::string text;
                                        int i = 0;
                                        REQUIRE(call.getArgument(0, u));
                                        REQUIRE(call.getArgument(0, d));
                                        REQUIRE(call.getArgument(0, b));
                                        REQUIRE(call.getArgument(0, text));
                                        REQUIRE(u == 3000000000u);
                                        REQUIRE(d == 3000000000.0);
                                        REQUIRE(b);
                                        REQUIRE(text == "3000000000");
                                        REQUIRE_THROWS_AS(call.getArgument(0, i), kern::RuntimeError);
                                        return true;
                                    }));
    REQUIRE(vm.load(s));
    REQUIRE(vm.callFunction("main"));
}
TEST_CASE("Host return contract rejects absent duplicate or void results", "[integration][vm][host]")
{
    for (int mode = 0; mode < 3; ++mode)
    {
        CAPTURE(mode);
        const auto kind = mode == 2 ? kern::ReturnKind::Void : kern::ReturnKind::Value;
        auto code = std::vector<Instruction>{instruction(Opcode::Calle)};
        if (kind == kern::ReturnKind::Value)
            code.push_back(instruction(Opcode::Pop));
        code.push_back(instruction(Opcode::Ret));
        auto s = hostProgram({0, kind}, code);
        VirtualMachine vm(nullptr);
        vm.addFunction("host", new Host({0, kind},
                                        [mode](kern::IExternCall &call)
                                        {
                                            if (mode)
                                            {
                                                call.setReturnValue(1);
                                                if (mode == 1)
                                                    call.setReturnValue(2);
                                            }
                                            return true;
                                        }));
        REQUIRE(vm.load(s));
        REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
    }
}
TEST_CASE("Full host signature is checked before callback side effects", "[integration][vm][host]")
{
    for (const auto sig :
         {kern::FunctionSignature{1, kern::ReturnKind::Void}, kern::FunctionSignature{0, kern::ReturnKind::Value}})
    {
        int calls = 0;
        auto s = hostProgram({0, kern::ReturnKind::Void}, {instruction(Opcode::Calle), instruction(Opcode::Ret)});
        VirtualMachine vm(nullptr);
        vm.addFunction("host", new Host(sig,
                                        [&](kern::IExternCall &)
                                        {
                                            ++calls;
                                            return true;
                                        }));
        REQUIRE(vm.load(s));
        REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
        REQUIRE(calls == 0);
    }
}
TEST_CASE("Host failures and exceptions leave the VM reusable", "[integration][vm][host]")
{
    for (int mode = 0; mode < 2; ++mode)
    {
        auto s = hostProgram({0, kern::ReturnKind::Void}, {instruction(Opcode::Calle), instruction(Opcode::Ret)});
        VirtualMachine vm(nullptr);
        vm.addFunction("host", new Host({0, kern::ReturnKind::Void},
                                        [mode](kern::IExternCall &) -> bool
                                        {
                                            if (mode)
                                                throw std::logic_error("host failed");
                                            return false;
                                        }));
        REQUIRE(vm.load(s));
        REQUIRE_THROWS(vm.callFunction("main"));
        vm.addFunction("host", new Host({0, kern::ReturnKind::Void}, [](kern::IExternCall &) { return true; }));
        REQUIRE(vm.callFunction("main"));
    }
}
TEST_CASE("Mutating the VM during dispatch is consistently refused", "[integration][vm][host]")
{
    for (int action = 0; action < 5; ++action)
    {
        auto s = hostProgram({0, kern::ReturnKind::Void}, {instruction(Opcode::Calle), instruction(Opcode::Ret)});
        VirtualMachine vm(nullptr);
        vm.addFunction("host", new Host({0, kern::ReturnKind::Void},
                                        [&](kern::IExternCall &)
                                        {
                                            switch (action)
                                            {
                                            case 0:
                                                vm.callFunction("main");
                                                break;
                                            case 1:
                                                vm.clearScripts();
                                                break;
                                            case 2:
                                                vm.removeFunction("host");
                                                break;
                                            case 3:
                                            {
                                                std::stringstream empty;
                                                vm.load(empty);
                                                break;
                                            }
                                            default:
                                                vm.resetProfile();
                                                break;
                                            }
                                            return true;
                                        }));
        REQUIRE(vm.load(s));
        REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
    }
}
TEST_CASE("Bindings survive script replacement and failed loads clear the program", "[integration][vm]")
{
    VirtualMachine vm(nullptr);
    int calls = 0;
    vm.addFunction("host", new Host({0, kern::ReturnKind::Void},
                                    [&](kern::IExternCall &)
                                    {
                                        ++calls;
                                        return true;
                                    }));
    for (int i = 0; i < 2; ++i)
    {
        auto s = hostProgram({0, kern::ReturnKind::Void}, {instruction(Opcode::Calle), instruction(Opcode::Ret)});
        REQUIRE(vm.load(s));
        REQUIRE(vm.callFunction("main"));
    }
    REQUIRE(calls == 2);
    std::stringstream corrupt("garbage");
    REQUIRE_FALSE(vm.load(corrupt));
    REQUIRE_FALSE(vm.callFunction("main"));
}
TEST_CASE("Uninitialized locals fail at runtime instead of becoming arbitrary values", "[integration][vm]")
{
    auto s = assembled("var x pushv x pop ret");
    VirtualMachine vm(nullptr);
    REQUIRE(vm.load(s));
    REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}
TEST_CASE("Numeric runtime errors are typed and do not wedge subsequent calls", "[integration][vm]")
{
    for (const std::string &body :
         {"pushi 1 pushi 0 div pop ret", "pushi 2147483647 inc pop ret", "pushs \"x\" pushi 1 add pop ret"})
    {
        auto s = assembled(body);
        VirtualMachine vm(nullptr);
        REQUIRE(vm.load(s));
        REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
        auto clean = assembled("ret");
        REQUIRE(vm.load(clean));
        REQUIRE(vm.callFunction("main"));
    }
}
TEST_CASE("Instruction limits stop before host side effects and allow exact termination", "[integration][vm][limits]")
{
    for (uint64_t budget : {0u, 1u})
    {
        kern::VMConfig config;
        config.maxExecutionSteps = budget;
        int calls = 0;
        auto code = std::vector<Instruction>{};
        if (budget)
            code.push_back(instruction(Opcode::Nop));
        code.push_back(instruction(Opcode::Calle));
        code.push_back(instruction(Opcode::Ret));
        auto s = hostProgram({0, kern::ReturnKind::Void}, code);
        VirtualMachine vm(nullptr, config);
        vm.addFunction("host", new Host({0, kern::ReturnKind::Void},
                                        [&](kern::IExternCall &)
                                        {
                                            ++calls;
                                            return true;
                                        }));
        REQUIRE(vm.load(s));
        REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
        REQUIRE(calls == 0);
    }
    kern::VMConfig config;
    config.maxExecutionSteps = 1;
    auto s = assembled("ret");
    VirtualMachine vm(nullptr, config);
    REQUIRE(vm.load(s));
    REQUIRE(vm.callFunction("main"));
}
TEST_CASE("Combined local and operand slots obey the stack budget", "[integration][vm][limits]")
{
    kern::VMConfig config;
    config.maxStackBytes = sizeof(Value);
    auto s = assembled("var x pushi 1 pop ret");
    VirtualMachine vm(nullptr, config);
    REQUIRE(vm.load(s));
    REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
    config.maxStackBytes = 0;
    VirtualMachine empty(nullptr, config);
    auto t = assembled("ret");
    REQUIRE(empty.load(t));
    REQUIRE(empty.callFunction("main"));
}
TEST_CASE("Huge frame and allocation configurations fail without attempting unbounded growth",
          "[integration][vm][limits]")
{
    auto s = bytes(program({instruction(Opcode::Ret)}, kern::ReturnKind::Void, UINT32_MAX));
    VirtualMachine vm(nullptr);
    REQUIRE(vm.load(s));
    REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
    kern::VMConfig config;
    config.maxStackBytes = SIZE_MAX;
    REQUIRE_THROWS_AS(VirtualMachine(nullptr, config), kern::RuntimeError);
}
TEST_CASE("Program budgets bound strings tables and instructions before allocation", "[integration][vm][limits]")
{
    kern::VMConfig config;
    config.maxProgramBytes = 256;
    auto p = program(std::vector<Instruction>(100, instruction(Opcode::Ret)));
    auto s = bytes(p);
    VirtualMachine vm(nullptr, config);
    REQUIRE_THROWS_AS(vm.load(s), kern::RuntimeError);
    REQUIRE_FALSE(vm.callFunction("main"));
    config.maxStringBytes = 3;
    p = program({instruction(Opcode::Ret)});
    p.strings = {"1234"};
    s = bytes(p);
    VirtualMachine strings(nullptr, config);
    REQUIRE_THROWS_AS(strings.load(s), kern::RuntimeError);
    config.maxTableEntries = 0;
    s = bytes(program({instruction(Opcode::Ret)}));
    VirtualMachine tables(nullptr, config);
    REQUIRE_THROWS_AS(tables.load(s), kern::RuntimeError);
}
TEST_CASE("Runtime strings share a bounded budget and release after calls and failures", "[integration][vm][limits]")
{
    kern::VMConfig config;
    config.maxRuntimeStringBytes = 4;
    VirtualMachine vm(nullptr, config);
    vm.addFunction("host", new Host({0, kern::ReturnKind::Value},
                                    [](kern::IExternCall &c)
                                    {
                                        c.setReturnValue(std::string("1234"));
                                        return true;
                                    }));
    auto s = hostProgram({0, kern::ReturnKind::Value},
                         {instruction(Opcode::Calle), instruction(Opcode::Calle), instruction(Opcode::Pop),
                          instruction(Opcode::Pop), instruction(Opcode::Ret)});
    REQUIRE(vm.load(s));
    REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
    s = hostProgram({0, kern::ReturnKind::Value},
                    {instruction(Opcode::Calle), instruction(Opcode::Pop), instruction(Opcode::Calle),
                     instruction(Opcode::Pop), instruction(Opcode::Ret)});
    REQUIRE(vm.load(s));
    REQUIRE(vm.callFunction("main"));
    REQUIRE(vm.callFunction("main"));
}
TEST_CASE("Embedded zero bytes survive constants locals and host returns", "[integration][vm]")
{
    const std::string text("a\0b", 3);
    auto p = program({instruction(Opcode::Movs, 0, 0), instruction(Opcode::Pushv, 0), instruction(Opcode::Calle, 0),
                      instruction(Opcode::Calle, 1), instruction(Opcode::Calle, 0), instruction(Opcode::Ret)},
                     kern::ReturnKind::Void, 1);
    p.strings = {text};
    p.externs = {{"println", {1, kern::ReturnKind::Void}}, {"text", {0, kern::ReturnKind::Value}}};
    auto s = bytes(p);
    VirtualMachine vm(nullptr);
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(vm, recorder);
    vm.addFunction("text", new Host({0, kern::ReturnKind::Value},
                                    [&](kern::IExternCall &c)
                                    {
                                        c.setReturnValue(text);
                                        return true;
                                    }));
    REQUIRE(vm.load(s));
    REQUIRE(vm.callFunction("main"));
    REQUIRE(recorder.text() == text + "\n" + text + "\n");
}
TEST_CASE("Moved VMs preserve the verified program budgets and bindings", "[integration][vm]")
{
    auto s = assembled("ret");
    VirtualMachine original(nullptr);
    REQUIRE(original.load(s));
    VirtualMachine moved(std::move(original));
    REQUIRE(moved.callFunction("main"));
    VirtualMachine assigned(nullptr);
    assigned = std::move(moved);
    REQUIRE(assigned.callFunction("main"));
}
TEST_CASE("Profiler counts canonical executed instructions including failure", "[integration][vm][profiling]")
{
    VirtualMachine vm(nullptr);
    vm.resetProfile();
    auto s = assembled("pushi 1 pop ret");
    REQUIRE(vm.load(s));
    REQUIRE(vm.callFunction("main"));
    std::stringstream out;
    vm.dumpProfile(out);
    REQUIRE(out.str().find("3 iterations") != std::string::npos);
    REQUIRE(out.str().find("pushi 1 ") != std::string::npos);
    vm.resetProfile();
    std::stringstream clear;
    vm.dumpProfile(clear);
    REQUIRE(clear.str().find("0 iterations") != std::string::npos);
}
TEST_CASE("Seeded malformed-bytecode mutations never crash or escape budgets", "[integration][vm][fuzz]")
{
    auto original =
        bytes(program({instruction(Opcode::Pushi, 7), instruction(Opcode::Pop), instruction(Opcode::Ret)})).str();
    std::mt19937 rng(12345);
    kern::VMConfig limits;
    limits.maxProgramBytes = 8192;
    limits.maxTableEntries = 128;
    limits.maxStringBytes = 128;
    limits.maxExecutionSteps = 32;
    limits.maxStackBytes = 1024;
    for (int i = 0; i < 500; ++i)
    {
        auto changed = original;
        const size_t offset = rng() % changed.size();
        changed[offset] = static_cast<char>(rng());
        std::stringstream s(changed);
        VirtualMachine vm(nullptr, limits);
        try
        {
            if (vm.load(s))
                vm.callFunction("main");
        }
        catch (const kern::RuntimeError &)
        {
        }
    }
}

TEST_CASE("Program accounting includes cached string objects even for empty constants", "[integration][vm][limits]")
{
    auto p = program({instruction(Opcode::Ret)});
    p.strings = {"", "", ""};
    kern::VMConfig config;
    config.maxProgramBytes =
        sizeof(ProgramFunction) + 4 + sizeof(Instruction) + p.strings.size() * (sizeof(std::string) + sizeof(Value));
    auto s = bytes(p);
    VirtualMachine vm(nullptr, config);
    REQUIRE_THROWS_AS(vm.load(s), kern::RuntimeError);
    REQUIRE_FALSE(vm.callFunction("main"));
}

TEST_CASE("Entry lookup preserves unsorted names internal call targets and signature checks",
          "[integration][vm][entry]")
{
    Program p;
    p.externs = {{"capture", {1, kern::ReturnKind::Void}}};
    for (int i = 0; i < 127; ++i)
    {
        const int value = (i * 37) % 127;
        p.functions.push_back(
            {"entry_" + std::to_string(value),
             {0, kern::ReturnKind::Void},
             0,
             {instruction(Opcode::Pushi, value), instruction(Opcode::Calle, 0), instruction(Opcode::Ret)},
             {}});
    }
    p.functions.push_back(
        {"wrapper", {0, kern::ReturnKind::Void}, 0, {instruction(Opcode::Call, 126), instruction(Opcode::Ret)}, {}});
    p.functions.push_back(
        {"valued", {0, kern::ReturnKind::Value}, 0, {instruction(Opcode::Pushi, 9), instruction(Opcode::Ret)}, {}});
    p.functions.push_back({"argument", {1, kern::ReturnKind::Void}, 1, {instruction(Opcode::Ret)}, {}});
    VirtualMachine vm(nullptr);
    int captured = -1;
    vm.addFunction("capture", new Host({1, kern::ReturnKind::Void},
                                       [&](kern::IExternCall &call) { return call.getArgument(0, captured); }));
    auto input = bytes(p);
    REQUIRE(vm.load(input));
    for (int value = 126; value >= 0; --value)
    {
        CAPTURE(value);
        REQUIRE(vm.callFunction("entry_" + std::to_string(value)));
        REQUIRE(captured == value);
    }
    REQUIRE(vm.callFunction("wrapper"));
    REQUIRE(captured == (126 * 37) % 127);
    REQUIRE_FALSE(vm.callFunction("entry_"));
    REQUIRE_FALSE(vm.callFunction("Entry_1"));
    REQUIRE_FALSE(vm.callFunction("entry_127"));
    REQUIRE_THROWS_AS(vm.callFunction("valued"), kern::RuntimeError);
    REQUIRE_THROWS_AS(vm.callFunction("argument"), kern::RuntimeError);
    REQUIRE(vm.callFunction("entry_1"));
    REQUIRE(captured == 1);
}

TEST_CASE("Entry lookup follows replacement clear failed loads and VM moves", "[integration][vm][entry]")
{
    auto make = [](const std::string &first, const std::string &second)
    {
        Program p;
        p.functions = {
            {first, {0, kern::ReturnKind::Void}, 0, {instruction(Opcode::Ret)}, {}},
            {second, {0, kern::ReturnKind::Value}, 0, {instruction(Opcode::Pushi, 1), instruction(Opcode::Ret)}, {}}};
        for (int i = 0; i < 32; ++i)
            p.functions.push_back(
                {"extra_" + std::to_string(i), {0, kern::ReturnKind::Void}, 0, {instruction(Opcode::Ret)}, {}});
        return bytes(p);
    };
    VirtualMachine original(nullptr);
    auto first = make("zebra", "alpha");
    REQUIRE(original.load(first));
    REQUIRE(original.callFunction("zebra"));
    REQUIRE_THROWS_AS(original.callFunction("alpha"), kern::RuntimeError);
    auto second = make("alpha", "zebra");
    REQUIRE(original.load(second));
    REQUIRE(original.callFunction("alpha"));
    REQUIRE_THROWS_AS(original.callFunction("zebra"), kern::RuntimeError);
    VirtualMachine moved(std::move(original));
    REQUIRE(moved.callFunction("alpha"));
    REQUIRE_THROWS_AS(moved.callFunction("zebra"), kern::RuntimeError);
    VirtualMachine assigned(nullptr);
    assigned = std::move(moved);
    REQUIRE(assigned.callFunction("alpha"));
    assigned.clearScripts();
    REQUIRE_FALSE(assigned.callFunction("alpha"));
    auto restored = make("zebra", "alpha");
    REQUIRE(assigned.load(restored));
    std::stringstream invalid("invalid bytecode");
    REQUIRE_FALSE(assigned.load(invalid));
    REQUIRE_FALSE(assigned.callFunction("zebra"));
    auto final = make("alpha", "zebra");
    REQUIRE(assigned.load(final));
    REQUIRE(assigned.callFunction("alpha"));
}

TEST_CASE("Entry lookup remains available at the exact existing program budget", "[integration][vm][entry][limits]")
{
    Program p;
    kern::VMConfig limits;
    limits.maxProgramBytes = 0;
    for (int i = 63; i >= 0; --i)
    {
        const std::string name = "entry_" + std::to_string(i);
        p.functions.push_back({name, {0, kern::ReturnKind::Void}, 0, {instruction(Opcode::Ret)}, {}});
        limits.maxProgramBytes += sizeof(ProgramFunction) + name.size() + sizeof(Instruction);
    }
    auto input = bytes(p);
    VirtualMachine vm(nullptr, limits);
    REQUIRE(vm.load(input));
    for (int i = 0; i < 64; ++i)
        REQUIRE(vm.callFunction("entry_" + std::to_string(i)));
    REQUIRE_FALSE(vm.callFunction("missing"));
}
