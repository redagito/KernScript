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

TEST_CASE("Compiler evaluates arithmetic precedence end-to-end", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var r = (3 + 4) * 2 - 10 / 2;\n  println(r);\n}\n";

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
  REQUIRE(recorder.text() == "9\n");
}

TEST_CASE("Compiler supports bare var declaration with later assignment", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var q;\n  q = 40 + 2;\n  println(q);\n}\n";

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
  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Compiler allows reassignment with another type", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var r = 1;\n  r = \"hi\";\n  println(r);\n}\n";

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
  REQUIRE(recorder.text() == "hi\n");
}

TEST_CASE("Compiler respects subtraction and division operand order", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n  var d = 10 - 2;\n  println(d);\n  var q = 10 / 2;\n  println(q);\n}\n";

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
  REQUIRE(recorder.text() == "8\n5\n");
}

TEST_CASE("Compiler rejects assignment to undeclared variable", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  q = 1;\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects duplicate var declaration", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var q = 1;\n  var q = 2;\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects call to undeclared extern", "[integration][compiler]") {
  const std::string source = "func main {\n  foo(42);\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}
