#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "testutil/TestSupport.hpp"

#include "common/Instruction.h"
#include "vm/VirtualMachine.h"

namespace
{

const std::string header = "extern println 1\n";

std::string wrap(const std::string &body)
{
  return header + "\nfunc main\n{\n" + body + "\n}\n";
}

/**
* \brief A function in a hand built byte code stream.
*
* Hand built streams carry corrupt indices the assembler would never emit,
* which is exactly what the tests below feed the virtual machine.
*/
struct FakeFunction
{
  std::string name = "main";
  uint32_t stackSize = 0;
  uint32_t parameterSize = 0;
  std::vector<Instruction> instructions;
};

template <typename T> void writeRaw(std::stringstream &stream, T value)
{
  stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
}

void writeString(std::stringstream &stream, const std::string &text)
{
  writeRaw<uint32_t>(stream, static_cast<uint32_t>(text.size()));
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

Instruction makeInstruction(Opcode id, int32_t arg0 = 0,
                             int32_t arg1 = 0)
{
  Instruction instruction{};
  instruction.id = id;
  instruction.args[0] = arg0;
  instruction.args[1] = arg1;
  return instruction;
}

/**
* \brief Builds a byte code stream in the layout load() reads.
*
* Only valid instruction ids are written here; the corruption under test is
* always in the argument values.
*/
std::stringstream buildByteCode(const std::vector<std::string> &strings,
                                const std::vector<FakeFunction> &functions)
{
  std::stringstream stream;
  writeRaw<uint32_t>(stream, static_cast<uint32_t>(strings.size()));
  for (const auto &text : strings)
  {
    writeString(stream, text);
  }
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, static_cast<uint32_t>(functions.size()));
  for (const auto &function : functions)
  {
    writeString(stream, function.name);
    writeRaw<uint32_t>(stream, function.stackSize);
    writeRaw<uint32_t>(stream, function.parameterSize);
    writeRaw<uint32_t>(stream,
                       static_cast<uint32_t>(function.instructions.size()));
    for (const auto &instruction : function.instructions)
    {
      writeRaw<uint16_t>(stream,
                         static_cast<uint16_t>(instruction.id));
      const unsigned int count =
          instructions[static_cast<unsigned int>(instruction.id)].argCount;
      for (unsigned int i = 0; i < count; ++i)
      {
        writeRaw<int32_t>(stream, instruction.args[i]);
      }
    }
  }
  return stream;
}

/**
* \brief Loads hand built byte code and runs main, expecting RuntimeError.
*/
void requireCorrupt(const std::vector<std::string> &strings,
                    const std::vector<FakeFunction> &functions)
{
  std::stringstream byteCode = buildByteCode(strings, functions);
  VirtualMachine vm(nullptr);
  REQUIRE(vm.load(byteCode));
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
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

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Inc on a string operand is refused", "[integration][vm]")
{
  const std::string source = wrap("pushs \"text\"\ninc\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Dec on an empty stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("dec\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
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

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Not on an empty stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("not\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("And on a short stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\nand\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
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

TEST_CASE("Jmp transfers control unconditionally", "[integration][vm]")
{
  const std::string source = wrap("jmp done\n"
                                  "pushi 20\ncalle println\n"
                                  "label done\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jmp to an unknown label fails to assemble", "[integration][vm]")
{
  const std::string source = wrap("jmp missing\nret");

  std::stringstream byteCode;
  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Jump if equal is taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 5\nje a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if equal is not taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 6\nje a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if equal compares across types", "[integration][vm]")
{
  const std::string source = wrap("pushi 2\npushf 2.0\nje a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if not equal is taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 6\njne a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if not equal is not taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 5\njne a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if greater is taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 6\npushi 5\njg a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if greater is not taken on equal values", "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 5\njg a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if greater compares across types", "[integration][vm]")
{
  const std::string source = wrap("pushf 2.5\npushi 2\njg a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if greater or equal is taken on equal values",
          "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 5\njge a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if greater or equal is not taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 4\npushi 5\njge a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if less is taken", "[integration][vm]")
{
  const std::string source = wrap("pushi 4\npushi 5\njl a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if less is not taken on equal values", "[integration][vm]")
{
  const std::string source = wrap("pushi 5\npushi 5\njl a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("retf returns the float to the caller", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc f\n{\nretf 2.5\n}\n"
                             "\nfunc main\n{\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2.5\n");
}

TEST_CASE("rets returns the string to the caller", "[integration][vm]")
{
  const std::string source =
      "extern println 1\n\nfunc f\n{\nrets \"returned\"\n}\n"
      "\nfunc main\n{\ncall f\n"
      "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "returned\n");
}

TEST_CASE("rets returns an empty string to the caller", "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc f\n{\nrets \"\"\n}\n"
                             "\nfunc main\n{\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "\n");
}

TEST_CASE("Break continues execution", "[integration][vm]")
{
  const std::string source = wrap("pushi 1\ncalle println\nbreak\n"
                                  "pushi 2\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n2\n");
}

TEST_CASE("reti from a function without parameters or locals",
          "[integration][vm]")
{
  const std::string source = "extern println 1\n\nfunc f\n{\nreti 5\n}\n"
                             "\nfunc main\n{\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "5\n");
}

TEST_CASE("Pop on an empty stack stops the script", "[integration][vm]")
{
  const std::string source = wrap("pop\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Calling an extern function without arguments stops the script",
          "[integration][vm]")
{
  const std::string source = wrap("calle println\npushi 99\n"
                                  "calle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  // The first call has nothing to pop, so the script stops there instead of
  // continuing to the push of 99.
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
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

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);

  REQUIRE(vm.load(byteCode));

  // The script stops at the division instead of producing a value.
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Arithmetic on a string operand is refused", "[integration][vm]")
{
  const std::string source =
      wrap("pushs \"text\"\npushi 1\nmul\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  // The script stops at the multiplication instead of producing a value.
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Add on string operands is refused", "[integration][vm][c1]")
{
  const std::string source =
      wrap("pushs \"text\"\npushi 1\nadd\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Sub on string operands is refused", "[integration][vm][c1]")
{
  const std::string source =
      wrap("pushi 1\npushs \"text\"\nsub\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Pushf keeps the exact float bit pattern", "[integration][vm][c3]")
{
  const std::string source = wrap("pushf 3.125\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3.125\n");
}

TEST_CASE("Pushu keeps the maximum unsigned value", "[integration][vm][c3]")
{
  const std::string source = wrap("pushu 4294967295\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "4294967295\n");
}

TEST_CASE("Locals do not leak across calls", "[integration][vm][c6]")
{
  const std::string source =
      "extern println 1\n\nfunc prime\n{\nvar v\nmovi v, 99\nret\n}\n"
      "\nfunc reader\n{\nvar w\ninc\ncalle println\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("prime"));

  // reader never writes w, so inc meets a non-numeric slot and stops the
  // script instead of printing prime's leftover 100.
  REQUIRE_THROWS_AS(vm.callFunction("reader"), kern::RuntimeError);
}

TEST_CASE("State is clean after a throw from a nested call",
          "[integration][vm][c6]")
{
  const std::string source =
      "extern println 1\n\nfunc inner\n{\npushi 1\npushi 0\ndiv\nret\n}\n"
      "\nfunc outer\n{\ncall inner\npushi 1\npushi 0\ndiv\nret\n}\n"
      "\nfunc clean\n{\npushi 42\ncalle println\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE_THROWS_AS(vm.callFunction("outer"), kern::RuntimeError);

  // A stale call frame would resume outer past its call after clean returns
  // and divide by zero there. A clean slate ends cleanly instead.
  REQUIRE(vm.callFunction("clean"));
  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("clearScripts drops the runtime and call stacks",
          "[integration][vm][c6]")
{
  const std::string source =
      "extern println 1\n\nfunc prime\n{\nvar v\nmovi v, 99\nret\n}\n"
      "\nfunc reader\n{\nvar w\ninc\ncalle println\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("prime"));

  vm.clearScripts();

  byteCode.clear();
  byteCode.seekg(0);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("reader"), kern::RuntimeError);
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
  // instead of continuing.
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  vm->removeFunction("println");

  REQUIRE_THROWS_AS(vm->callFunction("main"), kern::RuntimeError);
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

  // The script stops at the missing extern function, which now throws
  // instead of silently ending.
  REQUIRE_THROWS_AS(vm->callFunction("main"), kern::RuntimeError);

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

TEST_CASE("Passing a string to an int extern parameter throws",
          "[integration][vm]")
{
  const std::string source = "extern fast_fib 1\nextern println 1\n"
                             "\nfunc main\n{\npushs \"text\"\n"
                             "calle fast_fib\ncalle println\nret\n}\n";

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("VMConfig defaults to 1MB stack and 1024 depth",
          "[integration][vm][limits]")
{
  const kern::VMConfig config{};
  REQUIRE(config.maxStackBytes == 1024 * 1024);
  REQUIRE(config.maxCallDepth == 1024);
}

TEST_CASE("Stack limit allows pushes within budget",
          "[integration][vm][limits]")
{
  const size_t slots = 4;
  const kern::VMConfig config{slots * sizeof(Value), 1024};
  const std::string source = wrap("pushi 1\npushi 2\npushi 3\npushi 4\n"
                                  "calle println\ncalle println\n"
                                  "calle println\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));

  REQUIRE(recorder.text() == "4\n3\n2\n1\n");
}

TEST_CASE("Stack limit throws past budget", "[integration][vm][limits]")
{
  const size_t slots = 4;
  const kern::VMConfig config{slots * sizeof(Value), 1024};
  const std::string source = wrap("pushi 1\npushi 2\npushi 3\npushi 4\n"
                                  "pushi 5\npushi 6\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Call depth limit stops infinite recursion",
          "[integration][vm][limits]")
{
  const std::string source = "extern println 1\n\nfunc f\n{\ncall f\nret\n}\n"
                             "\nfunc main\n{\ncall f\nret\n}\n";
  const kern::VMConfig config{1024 * 1024, 4};

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Corrupt stack size is rejected, not bad_alloc",
          "[integration][vm][limits]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, 1)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 0xFFFFFFFF;
  callee.parameterSize = 0;

  std::stringstream byteCode = buildByteCode({}, {main, callee});
  VirtualMachine vm(nullptr, kern::VMConfig{});
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Factory overloads accept a config", "[integration][vm][limits]")
{
  const std::string source = wrap("pushi 1\npushi 2\npushi 3\npushi 4\n"
                                  "pushi 5\npushi 6\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  const kern::VMConfig tiny{4 * sizeof(Value), 1024};
  std::unique_ptr<kern::IVirtualMachine> vm(
      kern::createVirtualMachine(nullptr, tiny));
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  REQUIRE_THROWS_AS(vm->callFunction("main"), kern::RuntimeError);

  std::unique_ptr<kern::IVirtualMachine> defaulted(
      kern::createVirtualMachine(kern::VMConfig{}));
  testutil::StringRecorder output;
  testutil::registerHostFunctions(*defaulted, output);
  byteCode.clear();
  byteCode.seekg(0);
  REQUIRE(defaulted->load(byteCode));

  // A bug in the test itself would surface here as an unexpected throw.
  REQUIRE(defaulted->callFunction("main"));
}

TEST_CASE("Return past a full stack throws", "[integration][vm][limits]")
{
  const size_t slots = 2;
  const kern::VMConfig config{slots * sizeof(Value), 1024};
  const std::string source = "extern println 1\n\nfunc f\n{\nreti 5\n}\n"
                             "\nfunc main\n{\npushi 1\npushi 2\ncall f\n"
                             "calle println\nret\n}\n";

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  // Two pushes fill the budget and the call fits, but the valueless frame
  // leaves no slot for the return value.
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
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

TEST_CASE("Movv with a corrupt destination index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movv, 5, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movv with a corrupt source index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Movv, 0, 7)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movv with a negative index throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movv, -1, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movi with a corrupt destination index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movi, 4, 10)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movf with a corrupt destination index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movf, 4, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movs with a corrupt string index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Movs, 0, 4)};
  requireCorrupt({}, {main});
}

TEST_CASE("Pushv with a corrupt variable index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushv, 2)};
  requireCorrupt({}, {main});
}

TEST_CASE("Pushs with a corrupt string index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushs, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Popv with a corrupt destination index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Popv, 3)};
  requireCorrupt({}, {main});
}

TEST_CASE("Call with a corrupt function index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, 7)};
  requireCorrupt({}, {main});
}

TEST_CASE("Call with a negative function index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, -1)};
  requireCorrupt({}, {main});
}

TEST_CASE("Call with more parameters than pushed values throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, 1)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 1;
  callee.parameterSize = 5;
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Calle with a corrupt extern index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Calle, 3)};
  requireCorrupt({}, {main});
}

TEST_CASE("Calle with a negative extern index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Calle, -1)};
  requireCorrupt({}, {main});
}

TEST_CASE("Retv with a corrupt variable index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 0),
                       makeInstruction(Opcode::Call, 1),
                       makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 1;
  callee.instructions = {makeInstruction(Opcode::Retv, 9)};
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Rets with a corrupt string index throws",
          "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 0),
                       makeInstruction(Opcode::Call, 1),
                       makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.instructions = {makeInstruction(Opcode::Rets, 0)};
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Jmp past the end throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Jmp, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Je past the end throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Je, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jne past the end throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Pushi, 2),
                       makeInstruction(Opcode::Jne, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jg past the end throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 2),
                       makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Jg, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jge past the end throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Jge, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jl past the end throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Pushi, 2),
                       makeInstruction(Opcode::Jl, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jle past the end throws", "[integration][vm][corrupt]")
{
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Jle, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jmp to exactly the end returns cleanly",
          "[integration][vm][corrupt]")
{
  // A trailing label resolves to the instruction count itself, which the
  // fetch treats as an implicit return. That boundary stays legal.
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Jmp, 1)};
  std::stringstream byteCode = buildByteCode({}, {main});
  VirtualMachine vm(nullptr);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
}
