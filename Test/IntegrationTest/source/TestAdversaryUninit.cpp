#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>
#include <string>

#include <kern/Kern.h>
#include <kern/RuntimeError.h>

#include "testutil/TestSupport.hpp"

namespace
{

// Compiles source text, assembles it and runs "main", returning the asm text.
// All stages must succeed; failures fail the test instead of passing vacuously.
std::string compileAndAssemble(const std::string &source, std::stringstream &byteCode)
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE(compiler->compile(input, asmCode));
    const std::string asmText = asmCode.str();
    REQUIRE(testutil::assemble(asmText, byteCode));
    return asmText;
}

void requireScriptThrows(const std::string &source)
{
    std::stringstream byteCode;
    compileAndAssemble(source, byteCode);
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::DiscardRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE_THROWS_AS(vm->callFunction("main"), kern::RuntimeError);
}

void requireAsmThrows(const std::string &asmText)
{
    std::stringstream byteCode;
    REQUIRE(testutil::assemble(asmText, byteCode));
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::DiscardRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE_THROWS_AS(vm->callFunction("main"), kern::RuntimeError);
}

} // namespace

TEST_CASE("Folded copy of a never-assigned variable fails the run", "[integration][compiler][peephole]")
{
    // Lowering folds "pushv x; popv y" into "movv y, x". The unfolded pair
    // pushes an Invalid value and throws; the fold must throw the same way.
    requireScriptThrows("func main() {\n var x;\n var y = x;\n}\n");
}

TEST_CASE("Folded discard of a never-assigned variable fails the run", "[integration][compiler][peephole]")
{
    // An expression statement "x;" lowers to "pushv x; pop", which the
    // peephole drops. Dropping must not swallow the uninitialized read.
    requireScriptThrows("func main() {\n var x;\n x;\n}\n");
}

TEST_CASE("Folded return of a never-assigned variable fails the run", "[integration][compiler][peephole]")
{
    // "return x;" lowers to "pushv x; popv retTemp; retv retTemp", which the
    // peephole bypasses into "retv x". The bypass must throw the same way.
    requireScriptThrows("func f() {\n var x;\n return x;\n}\nfunc main() {\n f();\n}\n");
}

TEST_CASE("Movv of an uninitialized variable throws", "[integration][vm]")
{
    requireAsmThrows("func main\n{\nvar x\nvar y\nmovv y, x\nret\n}\n");
}

TEST_CASE("Retv of an uninitialized variable throws", "[integration][vm]")
{
    requireAsmThrows("func callee\n{\nvar x\nretv x\n}\nfunc main\n{\ncall callee\npop\nret\n}\n");
}
