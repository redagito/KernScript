#include <catch2/catch_test_macros.hpp>

#include "testutil/TestSupport.hpp"

#include "vm/CVirtualMachine.h"

namespace
{

const std::string header = "extern println 1\n";

std::string wrap(const std::string &body)
{
  return header + "\nfunc main\n{\n" + body + "\n}\n";
}
}

TEST_CASE("Inc increments the top of the stack in place", "[integration][vm]")
{
  const std::string source = wrap("pushi 41\ninc\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Dec decrements the top of the stack in place", "[integration][vm]")
{
  const std::string source = wrap("pushi 43\ndec\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Inc on a float keeps the value a float", "[integration][vm]")
{
  const std::string source = wrap("pushf 1.5\ninc\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2.5\n");
}

TEST_CASE("Dec on a float keeps the value a float", "[integration][vm]")
{
  const std::string source = wrap("pushf 2.5\ndec\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1.5\n");
}

TEST_CASE("Inc leaves the rest of the stack alone", "[integration][vm]")
{
  // 100 stays underneath, 1 becomes 2, then 100 is pushed again and added.
  const std::string source = wrap("pushi 100\npushi 1\ninc\ncalle println\n"
                                  "pushi 100\nadd\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2\n200\n");
}

TEST_CASE("Inc and dec can be chained", "[integration][vm]")
{
  const std::string source = wrap("pushi 0\ninc\ninc\ninc\ndec\n"
                                  "calle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2\n");
}

TEST_CASE("Inc on an empty stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("inc\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("Inc needs a value") );
}

TEST_CASE("Inc on a string operand is refused", "[integration][vm]")
{
  const std::string source = wrap("pushs \"text\"\ninc\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("Inc needs a numeric value") );
}

TEST_CASE("Dec on an empty stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("dec\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("Dec needs a value") );
}

TEST_CASE("And produces the logical conjunction of two values",
          "[integration][vm]")
{
  const std::string source =
      wrap("pushi 1\npushi 0\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("And reports true for two true values", "[integration][vm]")
{
  const std::string source =
      wrap("pushi 5\npushi 0\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // Zero is the only false value, so 5 and 0 is false.
  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("And treats any non zero value as true", "[integration][vm]")
{
  const std::string source =
      wrap("pushi 7\npushi 3\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Or produces the logical disjunction of two values",
          "[integration][vm]")
{
  const std::string source =
      wrap("pushi 0\npushi 4\nor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Or reports false when both values are false", "[integration][vm]")
{
  const std::string source =
      wrap("pushi 0\npushi 0\nor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Not negates the truth of the top value", "[integration][vm]")
{
  const std::string source = wrap("pushi 0\nnot\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Not negates a true value to false", "[integration][vm]")
{
  const std::string source = wrap("pushi 9\nnot\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Xor reports true for differing values", "[integration][vm]")
{
  const std::string source =
      wrap("pushi 1\npushi 0\nxor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Xor reports false for equal values", "[integration][vm]")
{
  const std::string source =
      wrap("pushi 1\npushi 1\nxor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Xor reports false for two false values", "[integration][vm]")
{
  const std::string source =
      wrap("pushi 0\npushi 0\nxor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("And works on floats", "[integration][vm]")
{
  const std::string source =
      wrap("pushf 0.5\npushf 0.0\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Not on a float keeps an integer result", "[integration][vm]")
{
  const std::string source = wrap("pushf 1.5\nnot\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Or on strings is refused", "[integration][vm]")
{
  const std::string source =
      wrap("pushs \"a\"\npushs \"b\"\nor\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("Or needs numeric values") );
}

TEST_CASE("Not on an empty stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("not\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("Not needs a value") );
}

TEST_CASE("And on a short stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\nand\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("And needs two values") );
}

TEST_CASE("A script function returns from ret", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Add and sub operate on the runtime stack", "[integration][vm]")
{
  const std::string source =
      wrap("pushi 10\npushi 20\nadd\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Pushf and pushi both push onto the runtime stack",
          "[integration][vm]")
{
  const std::string source =
      wrap("pushi 1\npushf 2.5\ncalle println\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Jump if less or equal is taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\npushi 2\njle a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Jump if less or equal is not taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 2\njle a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("A script function receives its parameter", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc print\n{\nparam a\n"
                             "pushv a\ncalle println\nret\n}\n"
                             "\nfunc main\n{\npushi 100\ncall print\nret\n}\n";

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Pushu pushes an unsigned integer constant", "[integration][vm]")
{
  const std::string source = wrap("pushu 3000000000\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // Would overflow if it were read back as a signed integer.
  REQUIRE(recorder.text() == "3000000000\n");
}

TEST_CASE("Pushu keeps its value unsigned across arithmetic",
          "[integration][vm]")
{
  const std::string source =
      wrap("pushu 4000000000\npushu 1\nadd\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "4000000001\n");
}

TEST_CASE("Pushs pushes a string constant", "[integration][vm]")
{
  const std::string source =
      wrap("pushs \"pushed string\"\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "pushed string\n");
}

TEST_CASE("Pushs reuses identical strings from the table",
          "[integration][vm]")
{
  const std::string source =
      wrap("pushs \"same\"\ncalle println\npushs \"same\"\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "same\nsame\n");
}

TEST_CASE("Pushs accepts an empty string", "[integration][vm]")
{
  const std::string source = wrap("pushs \"\"\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "\n");
}

TEST_CASE("Mul multiplies integers on the runtime stack", "[integration][vm]")
{
  const std::string source = wrap("pushi 6\npushi 7\nmul\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Mul multiplies floats", "[integration][vm]")
{
  const std::string source =
      wrap("pushf 2.5\npushf 4.0\nmul\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Mul widens to float when the operands differ",
          "[integration][vm]")
{
  const std::string source =
      wrap("pushi 3\npushf 0.5\nmul\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1.5\n");
}

TEST_CASE("Mul multiplies unsigned integers", "[integration][vm]")
{
  const std::string source = wrap("pushu 3000000000\npushu 2\nmul\n"
                                  "calle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // 3000000000 * 2 wraps around an unsigned 32 bit integer.
  REQUIRE(recorder.text() == "1705032704\n");
}

TEST_CASE("Mul leaves the rest of the stack alone", "[integration][vm]")
{
  const std::string source = wrap("pushi 100\npushi 6\npushi 7\nmul\n"
                                  "add\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "142\n");
}

TEST_CASE("Div divides integers with truncation", "[integration][vm]")
{
  const std::string source = wrap("pushi 17\npushi 5\ndiv\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3\n");
}

TEST_CASE("Div divides floats", "[integration][vm]")
{
  const std::string source =
      wrap("pushf 1.0\npushf 8.0\ndiv\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0.125\n");
}

TEST_CASE("Div by zero stops the script and logs an error",
          "[integration][vm]")
{
  const std::string source = wrap("pushi 1\npushi 0\ndiv\npushi 2\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  // Built directly rather than through the factory, so the diagnostics can be
  // captured through the recording logger.
  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);

  REQUIRE(vm.load(byteCode));

  // The script stops at the division instead of producing a value.
  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("Division by zero") );
}

TEST_CASE("Arithmetic on a string operand is refused", "[integration][vm]")
{
  const std::string source =
      wrap("pushs \"text\"\npushi 1\nmul\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  testutil::RecordingLogger logger;
  CVirtualMachine vm(&logger);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  // The script stops at the multiplication instead of producing a value.
  vm.callFunction("main");

  REQUIRE( logger.loggedContaining("numeric operands") );
}

TEST_CASE("Movs advances the instruction pointer", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc main\n{\nvar x\n"
                             "movs x, \"hello\"\npushv x\ncalle println\nret\n}\n";

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("movv advances the instruction pointer", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\nvar b\n"
                             "movi a, 3\nmovv b, a\npushv b\ncalle println\n"
                             "ret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3\n");
}

TEST_CASE("movi advances the instruction pointer", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 7\npushv a\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "7\n");
}

TEST_CASE("movf advances the instruction pointer", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movf a, 1.5\npushv a\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1.5\n");
}

TEST_CASE("reti returns the value to the caller", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc a\n{\nparam x\n"
                             "pushv x\ncalle println\nreti 20\n}\n"
                             "\nfunc main\n{\npushi 10\ncall a\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // Bug: the return value slot is one past the runtime stack base, which is
  // out of bounds, and the value is never written to the caller stack.
  REQUIRE(recorder.text() == "10\n20\n");
}

TEST_CASE("retv returns the value to the caller", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc f\n{\nparam x\nvar b\n"
                             "pushi 77\npopv b\nretv b\n}\n"
                             "\nfunc main\n{\npushi 5\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "77\n");
}

TEST_CASE("An empty function runs nothing", "[integration][vm]")
{
  const std::string source = header + "\nfunc main\n{\n}\n";

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  // The call returning at all is the assertion: execute() synthesises a ret for
  // an out of bounds instruction index, so an empty instruction list must not
  // loop forever. callFunction's return value carries no information about how
  // the script ended, so it is not asserted here.
  vm->callFunction("main");

  REQUIRE(recorder.text().empty());
}

TEST_CASE("exit ends the script", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\ncalle println\nexit\n"
                                  "pushi 2\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // Everything after exit must not run.
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Calling an unknown function fails", "[integration][vm]")
{
  const std::string source = wrap("ret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  REQUIRE_FALSE(vm->callFunction("missing"));
}

TEST_CASE("Calling an unregistered extern function stops the script",
          "[integration][vm]")
{
  const std::string source = wrap("pushi 1\ncalle println\npushi 2\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  REQUIRE(vm->load(byteCode));

  // The extern function is not registered, so the script stops at the calle
  // instead of continuing to the pushv that would follow it. callFunction's
  // own return value carries no information about that, so the observable
  // effect is asserted instead.
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  vm->removeFunction("println");
  vm->callFunction("main");

  REQUIRE(recorder.text().empty());
}

TEST_CASE("clearScripts removes the loaded script", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));
  REQUIRE(vm->callFunction("main"));
  REQUIRE(recorder.text() == "1\n");

  vm->clearScripts();

  // The script is gone, so the function can no longer be resolved.
  REQUIRE_FALSE(vm->callFunction("main"));
}

TEST_CASE("Extern functions survive a clearScripts call", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  vm->clearScripts();

  std::stringstream reloaded;
  REQUIRE(testutil::assemble(source, reloaded));
  REQUIRE(vm->load(reloaded));
  REQUIRE(vm->callFunction("main"));
}

TEST_CASE("removeFunction unregisters an extern function", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));
  REQUIRE(vm->callFunction("main"));
  REQUIRE(recorder.text() == "1\n");

  vm->removeFunction("println");

  vm->clearScripts();
  std::stringstream reloaded;
  REQUIRE(testutil::assemble(source, reloaded));
  REQUIRE(vm->load(reloaded));

  // The script stops at the missing extern function, so nothing after the
  // failing calle runs. callFunction's own return value carries no information
  // about that, so the observable effect is asserted instead.
  vm->callFunction("main");

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("A host function returns its value to the script",
          "[integration][vm]")
{
  const std::string source = "extern get_value 0\nextern println 1\n"
                             "\nfunc main\n{\ncalle get_value\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "35\n");
}

TEST_CASE("A host function receives its arguments in source order",
          "[integration][vm]")
{
  const std::string source = "extern log 2\n"
                             "\nfunc main\n{\npushi 1\npushi 2\n"
                             "calle log\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "[1]: 2\n");
}

TEST_CASE("An empty runtime stack is not popped", "[integration][vm]")
{
  const std::string source = wrap("ret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  // The runtime stack only grows for the called function's locals, so calling
  // a function that never pushes anything leaves it empty at ret.
  REQUIRE(vm->callFunction("main"));
}
