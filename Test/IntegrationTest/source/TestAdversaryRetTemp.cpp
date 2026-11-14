#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>
#include <string>

#include <kern/Kern.h>

#include "testutil/TestSupport.hpp"

namespace
{

// Compiles source text, assembles it and runs "main", returning what the
// script printed. Every stage must succeed; failures fail the test instead
// of passing vacuously.
std::string runSource(const std::string &source)
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE(compiler->compile(input, asmCode));

    std::stringstream byteCode;
    REQUIRE(testutil::assemble(asmCode.str(), byteCode));

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction("main"));
    return recorder.text();
}

} // namespace

TEST_CASE("Valued function returns through a local that resembles a spill name", "[integration][compiler][lower]")
{
    REQUIRE(
        runSource("extern println(x) -> void;\nfunc f(retmp) -> value {\n var retmp0 = retmp + 1;\n return retmp0;\n}\n"
                  "func main() -> void {\n println(f(41));\n}\n") == "42\n");
}

TEST_CASE("Valued function returns through similarly named locals and parameters", "[integration][compiler][lower]")
{
    REQUIRE(
        runSource("extern println(x) -> void;\nfunc f(retmp0) -> value {\n var retmp = retmp0 + 1;\n return retmp;\n}\n"
                  "func main() -> void {\n println(f(41));\n}\n") == "42\n");
}

TEST_CASE("Valued function returns through a local named retmp", "[integration][compiler][lower]")
{
    REQUIRE(runSource("extern println(x) -> void;\nfunc f() -> value {\n var retmp = 41;\n return retmp + 1;\n}\n"
                      "func main() -> void {\n println(f());\n}\n") == "42\n");
}

TEST_CASE("Valued function returns through a nested local named retmp", "[integration][compiler][lower]")
{
    REQUIRE(
        runSource(
            "extern println(x) -> void;\nfunc f(v) -> value {\n if (v) {\n var retmp = 41;\n return retmp + 1;\n }\n"
            " return 0;\n}\nfunc main() -> void {\n println(f(1));\n}\n") == "42\n");
}
