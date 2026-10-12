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

TEST_CASE("Compiler compiles extern println call with int literal end-to-end", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  println(42);\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE(compiler->compile(input, asmCode));

  const std::string asmText = asmCode.str();
  REQUIRE(asmText.find("extern println 1") != std::string::npos);
  REQUIRE(asmText.find("pushi 42") != std::string::npos);
  REQUIRE(asmText.find("calle println") != std::string::npos);

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(asmText, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));
  REQUIRE(vm->callFunction("main"));
  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Compiler rejects call to undeclared extern", "[integration][compiler]") {
  const std::string source = "func main {\n  foo(42);\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}
