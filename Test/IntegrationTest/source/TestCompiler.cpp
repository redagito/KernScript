#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

#include "testutil/TestSupport.hpp"

TEST_CASE("Compiler compiles empty main end-to-end", "[integration][compiler]")
{
    const std::string source = "func main() -> void {}";

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

TEST_CASE("Compiler compiles extern println call with int literal end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  println(42);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE(compiler->compile(input, asmCode));

    const std::string asmText = asmCode.str();

    std::stringstream byteCode;
    REQUIRE(testutil::assemble(asmText, byteCode));

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction("main"));
    REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Compiler produces executable bytecode directly", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  println(42);\n}\n";
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream byteCode;
    REQUIRE(compiler->compileBytecode(input, byteCode));

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction("main"));
    REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Compiler evaluates arithmetic precedence end-to-end", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var r = (3 + 4) * 2 - 10 / 2;\n  println(r);\n}\n";

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

TEST_CASE("Compiler rejects duplicate func declaration", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc foo() -> void {\n  println(1);\n}\nfunc foo() -> void {\n  "
        "println(2);\n}\nfunc main() -> void {\n  foo();\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects duplicate extern declaration", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects duplicate param name", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc foo(a, a) -> void {\n  println(a);\n}\nfunc main() -> "
                               "void {\n  foo(1, 2);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects func name colliding with extern", "[integration][compiler]")
{
    const std::string source =
        "extern foo(x) -> void;\nfunc foo(x) -> value {\n  return x;\n}\nfunc main() -> void {\n  foo(1);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects read of undeclared variable", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  println(q);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects call to undeclared script function as value", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var r = foo(1);\n  println(r);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects extern call with wrong argument count as statement", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  println(1, 2);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects extern call with wrong argument count as value", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var r = println(1, 2);\n  println(r);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects inconsistent extern use statement then value", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  println(1);\n  var r = println(2);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects overflowing integer literal", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var x = 4294967296;\n  println(x);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects overflowing float literal", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var f = "
                               "100000000000000000000000000000000000000000.0;\n  println(f);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects unterminated string literal", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var s = \"hello;\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects return outside a function", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\nreturn 1;\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler runs factorial and summation acceptance end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc fact(n) -> value {\n"
                               "  if (n == 1) {\n    return 1;\n  } else {\n"
                               "    var m = n - 1;\n"
                               "    var r = fact(m);\n"
                               "    return n * r;\n"
                               "  }\n"
                               "}\nfunc main() -> void {\n"
                               "  println(fact(5));\n"
                               "  var i = 1;\n"
                               "  var total = 0;\n"
                               "  while (i <= 10) {\n"
                               "    total = total + i;\n"
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
    REQUIRE(recorder.text() == "120\n55\n");
}

TEST_CASE("Compiler supports bare var declaration with later assignment", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var q;\n  q = 40 + 2;\n  println(q);\n}\n";

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

TEST_CASE("Compiler allows reassignment with another type", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var r = 1;\n  r = \"hi\";\n  println(r);\n}\n";

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

TEST_CASE("Compiler respects subtraction and division operand order", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var d = 10 - 2;\n  println(d);\n  "
                               "var q = 10 / 2;\n  println(q);\n}\n";

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

TEST_CASE("Compiler rejects assignment to undeclared variable", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  q = 1;\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects duplicate var declaration", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var q = 1;\n  var q = 2;\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler takes then-branch when condition holds", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var m = 3;\n  if (m == 3) {\n    println(1);\n  } "
        "else {\n    println(0);\n  }\n}\n";

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

TEST_CASE("Compiler takes else-branch when condition fails", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var m = 4;\n  if (m == 3) {\n    println(1);\n  } "
        "else {\n    println(0);\n  }\n}\n";

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

TEST_CASE("Compiler supports not-equal and ordered comparisons", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
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

TEST_CASE("Compiler runs then-block of if without else when taken", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var m = 3;\n  if (m == 3) {\n    "
                               "println(1);\n  }\n  println(2);\n}\n";

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

TEST_CASE("Compiler skips then-block of if without else when not taken", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var m = 4;\n  if (m == 3) {\n    "
                               "println(1);\n  }\n  println(2);\n}\n";

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

TEST_CASE("Compiler runs factorial while loop end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
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

TEST_CASE("Compiler skips while body when condition false initially", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
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

TEST_CASE("Compiler runs nested while loops end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
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

TEST_CASE("Compiler rejects call to undeclared extern", "[integration][compiler]")
{
    const std::string source = "func main() -> void {\n  foo(42);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler calls script function with param and return end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc double(v) -> value {\n  return v * 2;\n}\nfunc main() "
                               "-> void {\n  println(double(21));\n}\n";

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

TEST_CASE("Compiler calls void function as statement with bare return", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc show(v) -> void {\n  println(v);\n  return;\n}\n"
                               "func main() -> void {\n  show(7);\n  println(9);\n}\n";

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

TEST_CASE("Compiler evaluates nested calls as arguments end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc double(v) -> value {\n  return v * 2;\n}\n"
                               "func main() -> void {\n  println(double(double(2)));\n}\n";

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

TEST_CASE("Compiler runs shallow void recursion end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc count(n) -> void {\n"
                               "  if (n == 0) {\n    return;\n  }\n"
                               "  println(n);\n"
                               "  count(n - 1);\n"
                               "}\nfunc main() -> void {\n  count(5);\n}\n";

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

TEST_CASE("Compiler runs recursive valued function end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc fact(n) -> value {\n"
                               "  if (n == 1) {\n    return 1;\n  }\n"
                               "  var m = n - 1;\n"
                               "  var r = fact(m);\n"
                               "  return n * r;\n"
                               "}\nfunc main() -> void {\n  println(fact(5));\n}\n";

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

TEST_CASE("Compiler resolves forward script function references", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  println(double(21));\n}\n"
                               "func double(v) -> value {\n  return v * 2;\n}\n";

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

TEST_CASE("Compiler rejects mixed valued and void returns", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc f(v) -> value {\n  if (v == 1) {\n    return 1;\n  }\n  return;\n}\n"
        "func main() -> void {\n  println(f(2));\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects reachable fallthrough in a value function", "[integration][compiler]")
{
    const std::string source = "func maybe(v) -> value {\n  if (v) {\n    return 1;\n  }\n}\nfunc main() -> void {}\n";
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects void call used as a value", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc show(v) -> void {\n  println(v);\n}\n"
                               "func main() -> void {\n  var r = show(1);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects script call with wrong argument count", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc double(v) -> value {\n  return v * 2;\n}\n"
                               "func main() -> void {\n  println(double(1, 2));\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects inconsistent extern use", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var r = println(1);\n  println(2);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler prints string variable end-to-end", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var s = \"hello\";\n  println(s);\n}\n";

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

TEST_CASE("Compiler compares strings by value end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
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

TEST_CASE("Compiler compares strings with not-equal end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
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

TEST_CASE("Compiler evaluates float arithmetic end-to-end", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var f = 1.5 + 2.25;\n  println(f);\n}\n";

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

TEST_CASE("Compiler runs strings and floats goal end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
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

TEST_CASE("Compiler rejects string addition", "[integration][compiler]")
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());

    const std::string plusSource =
        "extern println(x) -> void;\nfunc main() -> void {\n  var s = \"a\" + \"b\";\n  println(s);\n}\n";
    {
        std::stringstream input(plusSource);
        std::stringstream asmCode;
        REQUIRE_FALSE(compiler->compile(input, asmCode));
    }
}

TEST_CASE("Compiler supports and or not goal end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
                               "  var a = 3;\n"
                               "  if ((a > 1) and (a < 5)) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
                               "  if (not (a == 4)) {\n    println(2);\n  }\n"
                               "  if ((a == 0) or (a == 3)) {\n    println(3);\n  }\n"
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
    REQUIRE(recorder.text() == "1\n2\n3\n");
}

TEST_CASE("Compiler supports xor end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
                               "  var a = 3;\n"
                               "  if ((a == 3) xor (a == 4)) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
                               "  if ((a == 3) xor (a == 3)) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
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

TEST_CASE("Compiler supports not on value end-to-end", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n"
                               "  var r = not 0;\n"
                               "  println(r);\n"
                               "  var s = not 5;\n"
                               "  println(s);\n"
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

TEST_CASE("Compiler respects logic precedence end-to-end", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n"
        "  var a = 1;\n"
        "  if (a == 1 or a == 2 and a == 3) {\n    println(1);\n  } else {\n    println(0);\n  }\n"
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
    REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Compiler rejects func without parentheses", "[integration][compiler]")
{
    const std::string source = "func main {}";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

namespace
{

std::string compileToAsm(const std::string &source)
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE(compiler->compile(input, asmCode));
    return asmCode.str();
}

std::string runSource(const std::string &source)
{
    const std::string asmText = compileToAsm(source);
    std::stringstream byteCode;
    REQUIRE(testutil::assemble(asmText, byteCode));

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction("main"));
    return recorder.text();
}

} // namespace

TEST_CASE("Compiler preserves integer local values", "[integration][compiler][lowering]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var n = 5;\n  println(n);\n}\n";
    REQUIRE(runSource(source) == "5\n");
}

TEST_CASE("Compiler preserves variable copy values", "[integration][compiler][lowering]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var a = 1;\n  var b = a;\n  println(b);\n}\n";
    REQUIRE(runSource(source) == "1\n");
}

TEST_CASE("Compiler preserves float and string local types", "[integration][compiler][lowering]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  var f = 1.5;\n  var s = \"hi\";\n "
                               " println(f);\n  println(s);\n}\n";
    REQUIRE(runSource(source) == "1.5\nhi\n");
}

TEST_CASE("Compiler preserves unsigned integer local values", "[integration][compiler][lowering]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var u = 3000000000;\n  println(u);\n}\n";
    REQUIRE(runSource(source) == "3000000000\n");
}

TEST_CASE("Compiler returns integer values through the canonical return", "[integration][compiler][lowering]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc f() -> value {\n  return 42;\n}\nfunc main() -> void {\n  println(f());\n}\n";
    REQUIRE(runSource(source) == "42\n");
}

TEST_CASE("Compiler returns variable values without hidden spill locals", "[integration][compiler][lowering]")
{
    const std::string source = "extern println(x) -> void;\nfunc id(v) -> value {\n  return v;\n}\nfunc main() -> void "
                               "{\n  println(id(7));\n}\n";
    REQUIRE(runSource(source) == "7\n");
}

TEST_CASE("Compiler evaluates and discards literal expression statements", "[integration][compiler][lowering]")
{
    const std::string source = "extern println(x) -> void;\nfunc main() -> void {\n  42;\n  println(1);\n}\n";
    REQUIRE(runSource(source) == "1\n");
}

TEST_CASE("Compiler rejects second module declaration", "[integration][compiler]")
{
    const std::string source = "module a;\nmodule b;\nfunc main() -> void {}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects module declaration after func", "[integration][compiler]")
{
    const std::string source = "func main() -> void {}\nmodule m;\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects module declaration after import", "[integration][compiler]")
{
    const std::string source = "import a;\nmodule m;\nfunc main() -> void {}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects import declaration after func", "[integration][compiler]")
{
    const std::string source = "func main() -> void {}\nimport a;\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects import declaration after extern", "[integration][compiler]")
{
    const std::string source = "extern f(x) -> void;\nimport a;\nfunc main() -> void {\n  f(1);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects export on extern declaration", "[integration][compiler]")
{
    const std::string source = "export extern f(x) -> void;\nfunc main() -> void {}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects export on non-func declaration", "[integration][compiler]")
{
    const std::string source = "export var x;\nfunc main() -> void {}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects doubly qualified call", "[integration][compiler]")
{
    const std::string source = "func main() -> void {\n  a::b::c();\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE_FALSE(compiler->compile(input, asmCode));
}

TEST_CASE("Compiler rejects call with empty qualifier", "[integration][compiler]")
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());

    const std::string stmtSource = "func main() -> void {\n  ::f();\n}\n";
    {
        std::stringstream input(stmtSource);
        std::stringstream asmCode;
        REQUIRE_FALSE(compiler->compile(input, asmCode));
    }

    const std::string exprSource = "func main() -> void {\n  var x = ::f();\n}\n";
    {
        std::stringstream input(exprSource);
        std::stringstream asmCode;
        REQUIRE_FALSE(compiler->compile(input, asmCode));
    }
}

TEST_CASE("Compiler rejects module import export used as identifiers", "[integration][compiler]")
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());

    const std::string funcName = "func module() -> void {}\n";
    {
        std::stringstream input(funcName);
        std::stringstream asmCode;
        REQUIRE_FALSE(compiler->compile(input, asmCode));
    }

    const std::string varName = "func main() -> void {\n  var import;\n}\n";
    {
        std::stringstream input(varName);
        std::stringstream asmCode;
        REQUIRE_FALSE(compiler->compile(input, asmCode));
    }

    const std::string paramName = "func f(export) -> void {\n}\nfunc main() -> void {}\n";
    {
        std::stringstream input(paramName);
        std::stringstream asmCode;
        REQUIRE_FALSE(compiler->compile(input, asmCode));
    }
}

namespace
{

// Slice 3 import tests keep the root program in memory (a stringstream) and
// load only imports from disk: each test passes a search path pointing at a
// directory of committed .ks fixtures (or at a temp dir it created at
// runtime). Fixture lookup tries data/ks next to the working directory first
// (the EndToEndTest copy_directory layout) and then walks up to the source
// tree, so the tests hold both under CTest and for manual runs from the repo
// root.
std::string ksFixtureDir()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::is_directory(fs::path("data/ks"), ec))
    {
        return "data/ks";
    }
    fs::path cwd = fs::current_path(ec);
    if (ec)
    {
        return std::string();
    }
    for (int i = 0; i < 10; ++i)
    {
        if (fs::is_directory(cwd / "Test/IntegrationTest/data/ks", ec))
        {
            return (cwd / "Test/IntegrationTest/data/ks").string();
        }
        if (!cwd.has_parent_path())
        {
            break;
        }
        cwd = cwd.parent_path();
    }
    return std::string();
}

struct ImportCompileResult
{
    bool ok = false;
    std::string asmText;
    std::vector<std::string> messages;
};

ImportCompileResult compileWithPaths(const std::string &source, const std::vector<std::string> &searchPaths,
                                     const std::string &sourceName = "<input>")
{
    testutil::RecordingLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    std::stringstream input(source);
    std::stringstream asmCode;
    const bool ok = compiler->compile(input, asmCode, sourceName, searchPaths);
    ImportCompileResult result;
    result.ok = ok;
    result.asmText = asmCode.str();
    result.messages = logger.messages();
    return result;
}

std::string runWithPaths(const std::string &source, const std::vector<std::string> &searchPaths,
                         const std::string &func = "main")
{
    const ImportCompileResult compiled = compileWithPaths(source, searchPaths);
    REQUIRE(compiled.ok);
    std::stringstream byteCode;
    REQUIRE(testutil::assemble(compiled.asmText, byteCode));

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction(func));
    return recorder.text();
}

bool contains(const std::vector<std::string> &messages, const std::string &part)
{
    for (const std::string &message : messages)
    {
        if (message.find(part) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

// Scratch directory removed on destruction, so temp-file tests leave no
// residue even when a REQUIRE aborts the case.
struct TempDir
{
    explicit TempDir(const std::string &name)
    {
        const auto project = std::filesystem::absolute(std::filesystem::path(__FILE__))
                                 .parent_path()
                                 .parent_path()
                                 .parent_path()
                                 .parent_path();
        root = (project / "build/test-tmp").lexically_normal();
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        REQUIRE_FALSE(ec);
        const auto token = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        // Exclusive creation isolates concurrently running build configurations.
        for (int attempt = 0; attempt < 32; ++attempt)
        {
            path = (root / (name + "-" + token + "-" + std::to_string(attempt))).lexically_normal();
            REQUIRE(path.is_absolute());
            REQUIRE(path.parent_path() == root);
            if (std::filesystem::create_directory(path, ec))
                return;
            REQUIRE_FALSE(ec);
        }
        FAIL("Cannot create an isolated test directory.");
    }
    ~TempDir()
    {
        if (path.is_absolute() && path.parent_path() == root)
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }
    void write(const std::string &name, const std::string &content)
    {
        std::ofstream out(path / name, std::ios::binary);
        out << content;
    }
    std::filesystem::path path;

private:
    std::filesystem::path root;
};

} // namespace

TEST_CASE("Compiler compiles module imports and export end-to-end", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source =
        "module mymod;\nimport other;\nextern println(x) -> void;\nexport func helper(v) -> value {\n"
        "  return v * 2;\n}\nfunc main() -> void {\n  println(helper(21));\n  println(other::triple(7));\n}\n";

    REQUIRE(runWithPaths(source, {dir}, "mymod@main") == "42\n21\n");
}

TEST_CASE("Compiler resolves a single import end-to-end", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(math::double(21));\n}\n";

    REQUIRE(runWithPaths(source, {dir}) == "42\n");
}

TEST_CASE("Compiler resolves a transitive import chain end-to-end", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source =
        "import c1;\nextern println(x) -> void;\nfunc main() -> void {\n  println(c1::f1());\n}\n";

    REQUIRE(runWithPaths(source, {dir}) == "12\n");
}

TEST_CASE("Compiler shares a diamond import once", "[integration][compiler]")
{
    // dia_u is reachable through both dia_a and dia_b. Loading it twice would
    // append fu twice and fail the merge, so success plus the correct output
    // proves it parsed once and was shared.
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import dia_a;\nimport dia_b;\nextern println(x) -> void;\nfunc main() -> void {\n  "
                               "println(dia_a::fa() + dia_b::fb());\n}\n";

    REQUIRE(runWithPaths(source, {dir}) == "23\n");
}

TEST_CASE("Compiler resolves the same import twice without duplication", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import math;\nimport math;\nextern println(x) -> void;\nfunc main() -> void {\n  "
                               "println(math::double(1));\n}\n";

    REQUIRE(runWithPaths(source, {dir}) == "2\n");
}

TEST_CASE("Compiler reports a self-import cycle", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import selfcyc;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "cycle"));
    REQUIRE(contains(result.messages, "selfcyc -> selfcyc"));
}

TEST_CASE("Compiler reports a two-module import cycle", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import cyc_a;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "cycle"));
    REQUIRE(contains(result.messages, "cyc_a -> cyc_b -> cyc_a"));
}

TEST_CASE("Compiler reports a three-module import cycle", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import loop1;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "cycle"));
    REQUIRE(contains(result.messages, "loop1 -> loop2 -> loop3 -> loop1"));
}

TEST_CASE("Compiler rejects a duplicate module name from a different file", "[integration][compiler]")
{
    // The root claims "dup" and so does dup.ks: two different files, one
    // module name. This must fail, never silently prefer one file.
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source =
        "module dup;\nimport dup;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "'dup'"));
    REQUIRE(contains(result.messages, "dup.ks"));
}

TEST_CASE("Compiler rejects an import whose file declares another module", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import wrong;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "wrong.ks"));
    REQUIRE(contains(result.messages, "notwrong"));
}

TEST_CASE("Compiler rejects an imported file without a module declaration", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import anon;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "anon.ks"));
    REQUIRE(contains(result.messages, "no module declaration"));
}

TEST_CASE("Compiler rejects an import with no search paths configured", "[integration][compiler]")
{
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(double(21));\n}\n";

    {
        testutil::RecordingLogger logger;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
        std::stringstream input(source);
        std::stringstream asmCode;
        REQUIRE_FALSE(compiler->compile(input, asmCode, "test.ks"));
        REQUIRE(contains(logger.messages(), "'math'"));
        REQUIRE(contains(logger.messages(), "search paths"));
    }
    {
        const ImportCompileResult result = compileWithPaths(source, {});
        REQUIRE_FALSE(result.ok);
        REQUIRE(contains(result.messages, "'math'"));
        REQUIRE(contains(result.messages, "search paths"));
    }
}

TEST_CASE("Compiler reports a missing import with the searched paths", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source =
        "import missing_mod;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "missing_mod.ks"));
    REQUIRE(contains(result.messages, dir));
}

TEST_CASE("Compiler matches import file names case-sensitively", "[integration][compiler]")
{
    // Only math.ks exists: a case-insensitive lookup would find it (and then
    // fail with a mismatch), so the missing-file error naming Math.ks proves
    // the exact match.
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import Math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "Math.ks"));
}

TEST_CASE("Compiler takes the first search path hit for an import", "[integration][compiler]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string first = dir + "/ord1";
    const std::string second = dir + "/ord2";
    const std::string source =
        "import which;\nextern println(x) -> void;\nfunc main() -> void {\n  println(which::whichfn());\n}\n";

    REQUIRE(runWithPaths(source, {first, second}) == "1\n");
    REQUIRE(runWithPaths(source, {second, first}) == "2\n");
}

TEST_CASE("Compiler still compiles anonymous and named roots without imports", "[integration][compiler]")
{
    const std::string anon = "extern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";
    const std::string named =
        "module standalone;\nextern println(x) -> void;\nfunc main() -> void {\n  println(2);\n}\n";

    for (const bool isNamed : {false, true})
    {
        const std::string &source = isNamed ? named : anon;
        const std::string entry = isNamed ? "standalone@main" : "main";
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
        REQUIRE(vm->callFunction(entry));
        REQUIRE_FALSE(recorder.text().empty());
    }
}

TEST_CASE("Compiler rejects an import chain past the depth bound", "[integration][compiler]")
{
    TempDir dir("kernscript_import_depth");
    const int depth = 150;
    for (int i = 0; i < depth; ++i)
    {
        const bool last = i + 1 >= depth;
        const std::string next = last ? "" : "import m" + std::to_string(i + 1) + ";\n";
        const std::string call = last ? "return " + std::to_string(i) + ";"
                                      : "return m" + std::to_string(i + 1) + "::f" + std::to_string(i + 1) + "() + " +
                                            std::to_string(i) + ";";
        dir.write("m" + std::to_string(i) + ".ks", "module m" + std::to_string(i) + ";\n" + next + "export func f" +
                                                       std::to_string(i) + "() -> value {\n  " + call + "\n}\n");
    }
    const std::string source =
        "import m0;\nextern println(x) -> void;\nfunc main() -> void {\n  println(m0::f0());\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "too deep"));
}

namespace
{

std::string compileFlatToAsm(const std::string &source)
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE(compiler->compile(input, asmCode, "test.ks"));
    return asmCode.str();
}

std::string runAsmText(const std::string &asmText, const std::string &func = "main")
{
    std::stringstream byteCode;
    REQUIRE(testutil::assemble(asmText, byteCode));

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction(func));
    return recorder.text();
}

void writeSliceMath(TempDir &dir)
{
    dir.write("math.ks", "module math;\n"
                         "export func double(v) -> value {\n"
                         "  return v * 2;\n"
                         "}\n"
                         "func helper(v) -> value {\n"
                         "  return v + 1;\n"
                         "}\n");
}

} // namespace

TEST_CASE("Anonymous source and assembly paths produce identical verified bytecode", "[integration][compiler][slice4]")
{
    const std::string emptyMain = "func main() -> void {}";
    const std::string factorial = "extern println(x) -> void;\nfunc fact(n) -> value {\n"
                                  "  if (n == 1) {\n    return 1;\n  }\n"
                                  "  var m = n - 1;\n"
                                  "  var r = fact(m);\n"
                                  "  return n * r;\n"
                                  "}\nfunc main() -> void {\n  println(fact(5));\n}\n";
    const std::string fib = "extern println(x) -> void;\nfunc fib(n) -> value {\n"
                            "  if (n < 2) {\n    return n;\n  }\n"
                            "  return fib(n - 1) + fib(n - 2);\n"
                            "}\nfunc main() -> void {\n  println(fib(10));\n}\n";

    for (const auto &source : {emptyMain, factorial, fib})
    {
        const auto assembly = compileFlatToAsm(source);
        REQUIRE(assembly.find('@') == std::string::npos);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
        std::stringstream input(source), direct, assembled;
        REQUIRE(compiler->compileBytecode(input, direct));
        REQUIRE(testutil::assemble(assembly, assembled));
        REQUIRE(direct.str() == assembled.str());
    }
    REQUIRE(runAsmText(compileFlatToAsm(factorial)) == "120\n");
    REQUIRE(runAsmText(compileFlatToAsm(fib)) == "55\n");
}

TEST_CASE("Slice4 qualified exported call runs end-to-end with mangled names", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_qualified");
    writeSliceMath(dir);
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(math::double(21));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()});
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "42\n");
}

TEST_CASE("Slice4 private helper runs unqualified inside its module", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_private_inside");
    dir.write("priv.ks", "module priv;\nexport func f(v) -> value {\n"
                         "  return helper(v) * 2;\n"
                         "}\nfunc helper(v) -> value {\n"
                         "  return v + 1;\n"
                         "}\n");
    const std::string source =
        "import priv;\nextern println(x) -> void;\nfunc main() -> void {\n  println(priv::f(5));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()});
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "12\n");
}

TEST_CASE("Slice4 qualified private call is rejected with an exact diagnostic", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_private_outside");
    writeSliceMath(dir);
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(math::helper(1));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()}, "main.ks");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.messages.size() == 2);
    REQUIRE(result.messages[0] == "main.ks:4:17: error: math::helper is not exported.");
    REQUIRE(result.messages[1] == "note: export it in math.ks or call it from inside math");
}

TEST_CASE("Slice4 unknown module is rejected with an exact diagnostic", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_unknown_module");
    writeSliceMath(dir);
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(nosuch::f(1));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()}, "main.ks");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.messages.size() == 2);
    REQUIRE(result.messages[0] == "main.ks:4:11: error: Unknown module 'nosuch'.");
    REQUIRE(result.messages[1] == "note: check the name or import it directly");
}

TEST_CASE("Slice4 unknown func is rejected with an exact diagnostic", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_unknown_func");
    writeSliceMath(dir);
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(math::nosuch(1));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()}, "main.ks");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.messages.size() == 2);
    REQUIRE(result.messages[0] == "main.ks:4:17: error: Unknown function 'nosuch' in module 'math'.");
    REQUIRE(result.messages[1] == "note: check the function name");
}

TEST_CASE("Slice4 unqualified cross-module call is rejected", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_unqualified");
    writeSliceMath(dir);
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(double(21));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()}, "main.ks");
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "Call to undeclared function."));
}

TEST_CASE("Slice4 qualified calls work in statement and expression positions", "[integration][compiler][slice4]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import vis_math;\nextern println(x) -> void;\nfunc main() -> void {\n"
                               "  vis_math::show(7);\n"
                               "  println(vis_math::add(vis_math::one(), 2));\n"
                               "}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "7\n3\n");
}

TEST_CASE("Slice4 arity and void violations are caught through qualified names", "[integration][compiler][slice4]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());

    const std::string aritySource = "import vis_math;\nextern println(x) -> void;\nfunc main() -> void {\n"
                                    "  println(vis_math::add(1));\n}\n";
    const ImportCompileResult arity = compileWithPaths(aritySource, {dir});
    REQUIRE_FALSE(arity.ok);
    REQUIRE(contains(arity.messages, "Wrong argument count."));
    REQUIRE(contains(arity.messages, "declared 2 parameters here"));

    const std::string voidSource = "import vis_math;\nextern println(x) -> void;\nfunc main() -> void {\n"
                                   "  var x = vis_math::show(1);\n}\n";
    const ImportCompileResult voidUse = compileWithPaths(voidSource, {dir});
    REQUIRE_FALSE(voidUse.ok);
    REQUIRE(contains(voidUse.messages, "Void call used as a value."));
}

TEST_CASE("Slice4 same func name in two modules stays distinct", "[integration][compiler][slice4]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import vis_math;\nimport vis_util;\nextern println(x) -> void;\nfunc main() -> void {\n"
                               "  println(vis_math::id(5));\n"
                               "  println(vis_util::id(5));\n"
                               "}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "105\n5\n");
}

TEST_CASE("Slice4 transitive import chain runs end-to-end through the VM", "[integration][compiler][slice4]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import vis_math;\nextern println(x) -> void;\nfunc main() -> void {\n"
                               "  println(vis_math::f(10));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir});
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "31\n");
}

TEST_CASE("Slice4 transitive module is invisible to the root", "[integration][compiler][slice4]")
{
    const std::string dir = ksFixtureDir();
    REQUIRE_FALSE(dir.empty());
    const std::string source = "import vis_math;\nextern println(x) -> void;\nfunc main() -> void {\n"
                               "  println(vis_util::g(1));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir}, "main.ks");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.messages.size() == 2);
    REQUIRE(result.messages[0] == "main.ks:4:11: error: Unknown module 'vis_util'.");
    REQUIRE(result.messages[1] == "note: check the name or import it directly");
}

TEST_CASE("Same-name externs merge on matching full signatures", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_extern_merge");
    dir.write("ea.ks", "module ea;\nextern println(x) -> void;\nexport func fa() -> value {\n"
                       "  println(1);\n"
                       "  return 1;\n"
                       "}\n");
    dir.write("eb.ks", "module eb;\nextern println(y) -> void;\nexport func fb() -> value {\n"
                       "  println(2);\n"
                       "  return 2;\n"
                       "}\n");
    const std::string source = "import ea;\nimport eb;\nextern println(x) -> void;\nfunc main() -> void {\n"
                               "  println(ea::fa() + eb::fb());\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()});
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "1\n2\n3\n");
}

TEST_CASE("Same-name externs conflict on arity mismatch", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_extern_conflict");
    dir.write("ec.ks", "module ec;\nextern println(x, y) -> void;\nexport func fc() -> value {\n  return 3;\n}\n");
    const std::string source = "import ec;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "Conflicting extern 'println'"));
}

TEST_CASE("Same-name externs conflict on return-kind mismatch", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_extern_return_conflict");
    dir.write("ec.ks", "module ec;\nextern println(x) -> value;\nexport func fc() -> void {}\n");
    const std::string source = "import ec;\nextern println(x) -> void;\nfunc main() -> void {}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()});
    REQUIRE_FALSE(result.ok);
    REQUIRE(contains(result.messages, "Conflicting extern 'println'"));
}

TEST_CASE("Slice4 qualified extern call is rejected with an exact diagnostic", "[integration][compiler][slice4]")
{
    TempDir dir("kernscript_slice4_qualified_extern");
    writeSliceMath(dir);
    const std::string source =
        "import math;\nextern println(x) -> void;\nfunc main() -> void {\n  println(math::println(1));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()}, "main.ks");
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.messages.size() == 2);
    REQUIRE(result.messages[0] == "main.ks:4:17: error: Extern 'println' has no module.");
    REQUIRE(result.messages[1] == "note: call externs unqualified, without '::'");
}

TEST_CASE("Slice4 named root main mangles but stays host-callable by mangled name", "[integration][compiler][slice4]")
{
    const std::string source = "module mymod;\nextern println(x) -> void;\nfunc main() -> void {\n  println(1);\n}\n";

    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE(compiler->compile(input, asmCode, "main.ks"));
    const std::string asmText = asmCode.str();
    REQUIRE_FALSE(asmText.empty());
    REQUIRE(runAsmText(asmText, "mymod@main") == "1\n");
}

TEST_CASE("Compiler supports script func names matching assembly words", "[integration][compiler]")
{
    const std::string source = "func label() -> value {\n  return 1;\n}\nfunc main() -> void {\n  label();\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {}, "main.ks");
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText).empty());
}

TEST_CASE("Compiler supports functions named call", "[integration][compiler]")
{
    const std::string source = "func call() -> void {\n}\nfunc main() -> void {\n  call();\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {}, "main.ks");
    REQUIRE(result.ok);
}

TEST_CASE("Compiler supports local names matching assembly words", "[integration][compiler]")
{
    const std::string source =
        "extern println(x) -> void;\nfunc main() -> void {\n  var pushv = 41;\n  println(pushv);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {}, "main.ks");
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "41\n");
}

TEST_CASE("Compiler supports parameter names matching assembly words", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void;\nfunc id(pushv) -> value {\n  return pushv;\n}\nfunc main() "
                               "-> void {\n  println(id(1));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {}, "main.ks");
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "1\n");
}

TEST_CASE("Compiler supports extern names matching assembly words", "[integration][compiler]")
{
    const std::string source = "extern label(x) -> void;\nfunc main() -> void {\n  label(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {}, "main.ks");
    REQUIRE(result.ok);
}

TEST_CASE("Compiler supports local names matching assembly words in modules", "[integration][compiler]")
{
    TempDir dir("kernscript_keyword_var_module");
    dir.write("kwmod.ks", "module kwmod;\nextern println(x) -> void;\nexport func go(v) -> void {\n  var pushv = v;\n"
                          "  println(pushv);\n}\n");
    const std::string source = "import kwmod;\nfunc main() -> void {\n  kwmod::go(1);\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()}, "main.ks");
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "1\n");
}

TEST_CASE("Compiler keeps keyword func names working inside named modules", "[integration][compiler]")
{
    TempDir dir("kernscript_keyword_func_module");
    dir.write("kwcalc.ks", "module kwcalc;\nexport func add(a, b) -> value {\n  return a + b;\n}\n");
    const std::string source =
        "import kwcalc;\nextern println(x) -> void;\nfunc main() -> void {\n  println(kwcalc::add(20, 22));\n}\n";

    const ImportCompileResult result = compileWithPaths(source, {dir.path.string()}, "main.ks");
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "42\n");
}

TEST_CASE("Compiler accepts nested extern calls with pending values", "[integration][compiler]")
{
    const std::vector<std::string> sources = {
        "extern println(x) -> void;\nextern get_value() -> value;\nfunc main() -> void {\n  println(0 + "
        "get_value());\n}\n",
        "extern println(x) -> void;\nextern get_value() -> value;\nfunc both(a, b) -> void {\n  println(a);\n}\n"
        "func main() -> void {\n  both(\"x\", get_value());\n}\n",
        "extern println(x) -> void;\nextern fast_fib(v) -> value;\nextern get_value() -> value;\nfunc main() -> void "
        "{\n"
        "  println(fast_fib(-get_value()));\n}\n",
        "extern println(x) -> void;\nextern get_value() -> value;\nfunc both(a, b) -> void {\n  println(a);\n}\n"
        "func name(id) -> value {\n  return \"n\";\n}\nfunc main() -> void {\n  both(name(1), get_value());\n}\n"};

    for (const std::string &source : sources)
    {
        const ImportCompileResult result = compileWithPaths(source, {}, "main.ks");
        REQUIRE(result.ok);
    }
}

TEST_CASE("Compiler keeps first-position nested extern calls working", "[integration][compiler]")
{
    const std::string first =
        "extern println(x) -> void;\nextern get_value() -> value;\nfunc main() -> void {\n  println(get_value());\n}\n";
    REQUIRE(runWithPaths(first, {}) == "35\n");

    const std::string head =
        "extern println(x) -> void;\nextern get_value() -> value;\nfunc both(a, b) -> void {\n  println(a);\n}\n"
        "func main() -> void {\n  both(get_value(), \"x\");\n}\n";
    REQUIRE(runWithPaths(head, {}) == "35\n");

    const std::string cond =
        "extern println(x) -> void;\nextern get_value() -> value;\nfunc main() -> void {\n  if (get_value() == 35) {\n"
        "    println(\"y\");\n  }\n}\n";
    REQUIRE(runWithPaths(cond, {}) == "y\n");
}

TEST_CASE("Imported lowering errors identify the imported source", "[integration][compiler]")
{
    TempDir dir("kernscript_import_diagnostic");
    dir.write("broken.ks", "module broken;\nexport func value() -> value {\n  return missing;\n}\n");
    const auto result = compileWithPaths("import broken;\nfunc main() -> void {}\n", {dir.path.string()}, "root.ks");
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.messages.empty());
    REQUIRE(result.messages[0].find("broken.ks:3:") != std::string::npos);
    REQUIRE(result.messages[0].find("root.ks:") == std::string::npos);
}

TEST_CASE("Nested extern expressions execute in source order", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void; extern log(a,b) -> void; extern get_value() -> value;\n"
                               "func identity(x) -> value { return x; }\n"
                               "func main() -> void {\n"
                               " println(1 + get_value());\n"
                               " println(-get_value());\n"
                               " log(\"tag\", get_value());\n"
                               " println(identity(2) + get_value());\n"
                               " println(get_value() + get_value());\n"
                               "}\n";
    const auto result = compileWithPaths(source, {});
    REQUIRE(result.ok);
    REQUIRE(runAsmText(result.asmText) == "36\n-35\n[tag]: 35\n37\n70\n");
}

TEST_CASE("Assembly and direct compilation preserve small floats exactly", "[integration][compiler]")
{
    const std::string source = "extern println(x) -> void; func main() -> void { println(0.0000000000001); "
                               "println(-0.0); println(340282346638528859811704183484516925440.0); }";
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
    std::stringstream a(source), b(source), assembly, direct, assembled;
    REQUIRE(compiler->compile(a, assembly));
    REQUIRE(compiler->compileBytecode(b, direct));
    REQUIRE(testutil::assemble(assembly.str(), assembled));
    REQUIRE(direct.str() == assembled.str());
}
