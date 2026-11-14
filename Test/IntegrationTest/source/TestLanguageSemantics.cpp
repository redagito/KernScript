#include <kern/Kern.h>
#include "testutil/TestSupport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <memory>
#include <sstream>
#include <vector>

namespace
{
class Observe final : public kern::IExternFunction
{
    std::function<void(kern::IExternCall &)> action;
public:
    explicit Observe(std::function<void(kern::IExternCall &)> fn) : action(std::move(fn)) {}
    kern::FunctionSignature getSignature() const override { return {1, kern::ReturnKind::Void}; }
    bool call(kern::IExternCall &call) override { action(call); return true; }
};

void execute(const std::string &source, bool direct, std::function<void(kern::IExternCall &)> observe)
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source), bytes, assembly;
    if (direct)
        REQUIRE(compiler->compileBytecode(input, bytes));
    else
    {
        REQUIRE(compiler->compile(input, assembly));
        REQUIRE(testutil::assemble(assembly.str(), bytes));
    }
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    vm->addFunction("observe", new Observe(std::move(observe)));
    REQUIRE(vm->load(bytes));
    const bool succeeded = vm->callFunction("main");
    REQUIRE(succeeded);
}

int orderedSix(int a, int b, int c, int d, int e, int f)
{
    REQUIRE(a == 1); REQUIRE(b == 2); REQUIRE(c == 3);
    REQUIRE(d == 4); REQUIRE(e == 5); REQUIRE(f == 6);
    return a + 10*b + 100*c + 1000*d + 10000*e + 100000*f;
}

int mixedSix(const std::string &text, int integer, float real, unsigned int u, double d, bool b)
{
    REQUIRE(text == "text"); REQUIRE(integer == -2); REQUIRE(real == 3.5f);
    REQUIRE(u == 4000000000u); REQUIRE(d == 4.25); REQUIRE(b);
    return 42;
}
}

TEST_CASE("Conditions and logical operators share truth semantics", "[integration][compiler][vm]")
{
    const std::string source = R"(extern observe(x) -> void;
func main() -> void {
  if ("x") { observe(1); } else { observe(0); }
  if ("") { observe(1); } else { observe(0); }
  observe(not "x"); observe(not "");
  observe("x" and 2); observe("" or 0); observe("x" xor 1);
  observe(0 and "x"); observe(0 or "x"); observe("x" xor 0);
  var condition = "x";
  while (condition) { observe(1); condition = 0; }
  if (-0.0) { observe(9); } else { observe(0); }
  observe(not -0.0); observe(not 0); observe(not -2);
})";
    for (bool direct : {false, true})
    {
        std::vector<int> observed;
        execute(source, direct, [&](kern::IExternCall &call) {
            int value = -1; REQUIRE(call.getArgument(0, value)); observed.push_back(value);
        });
        REQUIRE(observed == std::vector<int>{1,1,0,0,1,1,0,0,1,1,1,0,1,1,0});
    }
}

TEST_CASE("Compiled numeric negation preserves signs and integer limits", "[integration][compiler][vm]")
{
    const std::string source = R"(extern observe(x) -> void;
func negate(x) -> value { return -x; }
func main() -> void {
  observe(-0.0); observe(negate(-0.0)); observe(-1.5);
  observe(-2147483648); observe(-(-2147483647));
})";
    for (bool direct : {false, true})
    {
        std::vector<double> observed;
        execute(source, direct, [&](kern::IExternCall &call) {
            double value = 0; REQUIRE(call.getArgument(0, value)); observed.push_back(value);
        });
        REQUIRE(observed.size() == 5);
        REQUIRE(observed[0] == 0); REQUIRE(std::signbit(observed[0]));
        REQUIRE(observed[1] == 0); REQUIRE_FALSE(std::signbit(observed[1]));
        REQUIRE(observed[2] == -1.5); REQUIRE(observed[3] == -2147483648.0);
        REQUIRE(observed[4] == 2147483647.0);
        for (const auto &expr : {"-(-2147483648)", "-2147483649"})
            REQUIRE_THROWS_AS(execute(std::string("func main() -> void { ") + expr + "; }", direct,
                                     [](kern::IExternCall &) {}), kern::RuntimeError);
    }
}

TEST_CASE("Typed registration handles larger mixed signatures and checks conversions", "[integration][binding]")
{
    const std::string source = R"(extern six(a,b,c,d,e,f) -> value;
extern mixed(a,b,c,d,e,f) -> value;
func main() -> void { six(1,2,3,4,5,6); mixed("text",-2,3.5,4000000000,4.25,1); })";
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source), bytes;
    REQUIRE(compiler->compileBytecode(input, bytes));
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    vm->addFunction("six", &orderedSix);
    vm->addFunction("mixed", &mixedSix);
    REQUIRE(vm->load(bytes)); REQUIRE(vm->callFunction("main"));
    REQUIRE_THROWS_AS(vm->addFunction("bad", static_cast<void (*)()>(nullptr)), kern::RuntimeError);
    REQUIRE_THROWS_AS(vm->addFunction("", &orderedSix), kern::RuntimeError);
    for (const auto &args : {"\"text\",\"bad\",3.5,4,4.25,1", "\"text\",4000000000,3.5,4,4.25,1"})
    {
        std::stringstream badInput(std::string("extern mixed(a,b,c,d,e,f) -> value; func main() -> void { mixed(") + args + "); }"), badBytes;
        REQUIRE(compiler->compileBytecode(badInput, badBytes));
        REQUIRE(vm->load(badBytes));
        REQUIRE_THROWS_AS(vm->callFunction("main"), kern::RuntimeError);
    }
}
