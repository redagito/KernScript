#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>
#include <string>

#include <kern/Kern.h>

#include "testutil/TestSupport.hpp"

TEST_CASE("Compiler compiles empty main end-to-end", "[integration][compiler]") {
  const std::string source = "func main {}";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE(compiler->compile(input, asmCode));

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(asmCode.str(), byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));
  REQUIRE(vm->callFunction("main"));
}
