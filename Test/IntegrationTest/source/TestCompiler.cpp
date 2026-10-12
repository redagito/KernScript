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

TEST_CASE("Compiler takes then-branch when condition holds", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var m = 3;\n  if (m == 3) {\n    println(1);\n  } else {\n    println(0);\n  }\n}\n";

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
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Compiler takes else-branch when condition fails", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var m = 4;\n  if (m == 3) {\n    println(1);\n  } else {\n    println(0);\n  }\n}\n";

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
  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Compiler supports not-equal and ordered comparisons", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n"
      "  var m = 4;\n"
      "  if (m != 3) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "  if (m < 5) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "  if (m > 5) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "  if (m <= 4) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "  if (m >= 5) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "}\n";

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
  REQUIRE(recorder.text() == "1\n1\n0\n1\n0\n");
}

TEST_CASE("Compiler runs then-block of if without else when taken", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var m = 3;\n  if (m == 3) {\n    println(1);\n  }\n  println(2);\n}\n";

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
  REQUIRE(recorder.text() == "1\n2\n");
}

TEST_CASE("Compiler skips then-block of if without else when not taken", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var m = 4;\n  if (m == 3) {\n    println(1);\n  }\n  println(2);\n}\n";

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
  REQUIRE(recorder.text() == "2\n");
}

TEST_CASE("Compiler rejects logic operators in this slice", "[integration][compiler]") {
  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());

  const std::string andSource = "extern println(x);\nfunc main {\n  var m = 3;\n  if (m == 3 and m < 5) {\n    println(1);\n  }\n}\n";
  {
    std::stringstream input(andSource);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
  }
}

TEST_CASE("Compiler runs factorial while loop end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n"
      "  var n = 5;\n"
      "  var acc = 1;\n"
      "  while (n > 1) {\n"
      "    acc = acc * n;\n"
      "    n = n - 1;\n"
      "  }\n"
      "  println(acc);\n"
      "}\n";

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
  REQUIRE(recorder.text() == "120\n");
}

TEST_CASE("Compiler skips while body when condition false initially", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n"
      "  var n = 0;\n"
      "  while (n > 1) {\n"
      "    println(1);\n"
      "  }\n"
      "  println(2);\n"
      "}\n";

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
  REQUIRE(recorder.text() == "2\n");
}

TEST_CASE("Compiler runs nested while loops end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n"
      "  var i = 0;\n"
      "  var total = 0;\n"
      "  while (i < 2) {\n"
      "    var j = 0;\n"
      "    while (j < 3) {\n"
      "      total = total + 1;\n"
      "      j = j + 1;\n"
      "    }\n"
      "    i = i + 1;\n"
      "  }\n"
      "  println(total);\n"
      "}\n";

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
  REQUIRE(recorder.text() == "6\n");
}

TEST_CASE("Compiler rejects call to undeclared extern", "[integration][compiler]") {
  const std::string source = "func main {\n  foo(42);\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler calls script function with param and return end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc double(v) {\n  return v * 2;\n}\nfunc main {\n  println(double(21));\n}\n";

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

TEST_CASE("Compiler calls void function as statement with bare return", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc show(v) {\n  println(v);\n  return;\n}\n"
      "func main {\n  show(7);\n  println(9);\n}\n";

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
  REQUIRE(recorder.text() == "7\n9\n");
}

TEST_CASE("Compiler evaluates nested calls as arguments end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc double(v) {\n  return v * 2;\n}\n"
      "func main {\n  println(double(double(2)));\n}\n";

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
  REQUIRE(recorder.text() == "8\n");
}

TEST_CASE("Compiler runs shallow void recursion end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc count(n) {\n"
      "  if (n == 0) {\n    return;\n  }\n"
      "  println(n);\n"
      "  count(n - 1);\n"
      "}\nfunc main {\n  count(5);\n}\n";

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
  REQUIRE(recorder.text() == "5\n4\n3\n2\n1\n");
}

TEST_CASE("Compiler runs recursive valued function end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc fact(n) {\n"
      "  if (n == 1) {\n    return 1;\n  }\n"
      "  var m = n - 1;\n"
      "  var r = fact(m);\n"
      "  return n * r;\n"
      "}\nfunc main {\n  println(fact(5));\n}\n";

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
  REQUIRE(recorder.text() == "120\n");
}

TEST_CASE("Compiler resolves forward script function references", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n  println(double(21));\n}\n"
      "func double(v) {\n  return v * 2;\n}\n";

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

TEST_CASE("Compiler rejects mixed valued and void returns", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc f(v) {\n  if (v == 1) {\n    return 1;\n  }\n  return;\n}\n"
      "func main {\n  println(f(2));\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects void call used as a value", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc show(v) {\n  println(v);\n}\n"
      "func main {\n  var r = show(1);\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects script call with wrong argument count", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc double(v) {\n  return v * 2;\n}\n"
      "func main {\n  println(double(1, 2));\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects inconsistent extern use", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n  var r = println(1);\n  println(2);\n}\n";

  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
  std::stringstream input(source);
  std::stringstream asmCode;
  REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler prints string variable end-to-end", "[integration][compiler]") {
  const std::string source = "extern println(x);\nfunc main {\n  var s = \"hello\";\n  println(s);\n}\n";

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
  REQUIRE(recorder.text() == "hello\n");
}

TEST_CASE("Compiler compares strings by value end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n"
      "  var s = \"hello\";\n"
      "  if (s == \"hello\") {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "  if (s == \"bye\") {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "}\n";

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
  REQUIRE(recorder.text() == "1\n0\n");
}

TEST_CASE("Compiler compares strings with not-equal end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n"
      "  var s = \"hello\";\n"
      "  if (s != \"bye\") {\n    println(2);\n  } else {\n    println(0);\n  }\n"
      "  if (s != \"hello\") {\n    println(2);\n  } else {\n    println(0);\n  }\n"
      "}\n";

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
  REQUIRE(recorder.text() == "2\n0\n");
}

TEST_CASE("Compiler evaluates float arithmetic end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n  var f = 1.5 + 2.25;\n  println(f);\n}\n";

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
  REQUIRE(recorder.text() == "3.75\n");
}

TEST_CASE("Compiler runs strings and floats goal end-to-end", "[integration][compiler]") {
  const std::string source =
      "extern println(x);\nfunc main {\n"
      "  var s = \"hello\";\n"
      "  println(s);\n"
      "  if (s == \"hello\") {\n    println(1);\n  } else {\n    println(0);\n  }\n"
      "  if (s != \"bye\") {\n    println(2);\n  }\n"
      "  var f = 1.5 + 2.25;\n"
      "  println(f);\n"
      "}\n";

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
  REQUIRE(recorder.text() == "hello\n1\n2\n3.75\n");
}

TEST_CASE("Compiler rejects string addition", "[integration][compiler]") {
  std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());

  const std::string plusSource = "extern println(x);\nfunc main {\n  var s = \"a\" + \"b\";\n  println(s);\n}\n";
  {
    std::stringstream input(plusSource);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
  }
}
