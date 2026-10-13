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

TEST_CASE("Valued function keeps user variable beside hidden spill slot", "[integration][compiler][lower]")
{
    // The hidden return spill slot is picked before the body lowers. A
    // parameter named retmp must not steal retmp0 from the body.
    REQUIRE(runSource("extern println(x);\nfunc f(retmp) {\n var retmp0 = retmp + 1;\n return retmp0;\n}\n"
                      "func main() {\n println(f(41));\n}\n") == "42\n");
}

TEST_CASE("Valued function keeps retmp variable beside retmp0 parameter", "[integration][compiler][lower]")
{
    REQUIRE(runSource("extern println(x);\nfunc f(retmp0) {\n var retmp = retmp0 + 1;\n return retmp;\n}\n"
                      "func main() {\n println(f(41));\n}\n") == "42\n");
}

TEST_CASE("Valued function may declare its own retmp variable", "[integration][compiler][lower]")
{
    REQUIRE(runSource("extern println(x);\nfunc f() {\n var retmp = 41;\n return retmp + 1;\n}\n"
                      "func main() {\n println(f());\n}\n") == "42\n");
}

TEST_CASE("Valued function may declare retmp inside nested blocks", "[integration][compiler][lower]")
{
    REQUIRE(runSource("extern println(x);\nfunc f(v) {\n if (v) {\n var retmp = 41;\n return retmp + 1;\n }\n"
                      " return 0;\n}\nfunc main() {\n println(f(1));\n}\n") == "42\n");
}
