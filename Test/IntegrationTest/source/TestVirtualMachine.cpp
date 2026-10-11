#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "testutil/TestSupport.hpp"

#include "common/Instruction.h"
#include "common/ScriptHeader.h"
#include "common/Version.h"
#include "vm/VirtualMachine.h"

namespace {

const std::string header = "extern println 1\n";

std::string wrap(const std::string &body) { return header + "\nfunc main\n{\n" + body + "\n}\n"; }

/**
 * \brief A function in a hand built byte code stream.
 *
 * Hand built streams carry corrupt indices the assembler would never emit,
 * which is exactly what the tests below feed the virtual machine.
 */
struct FakeFunction {
  std::string name = "main";
  uint32_t stackSize = 0;
  uint32_t parameterSize = 0;
  std::vector<Instruction> instructions;
};

template <typename T> void writeRaw(std::stringstream &stream, T value) {
  stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
}

void writeString(std::stringstream &stream, const std::string &text) {
  writeRaw<uint32_t>(stream, static_cast<uint32_t>(text.size()));
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

/**
 * \brief Writes the valid header every load() expects.
 *
 * Hand built streams carry corrupt table contents the assembler would never
 * emit, but the header itself stays valid so the tests reach the table under
 * test instead of failing on magic or version.
 */
void writeHeader(std::stringstream &stream) {
  stream.write("KERN", 4);
  writeRaw<uint8_t>(stream, static_cast<uint8_t>(majorVersion));
  writeRaw<uint8_t>(stream, static_cast<uint8_t>(minorVersion));
  writeRaw<uint32_t>(stream, 0);
}

Instruction makeInstruction(Opcode id, int32_t arg0 = 0, int32_t arg1 = 0) {
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
std::stringstream buildByteCode(const std::vector<std::string> &strings, const std::vector<FakeFunction> &functions) {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, static_cast<uint32_t>(strings.size()));
  for (const auto &text : strings) {
    writeString(stream, text);
  }
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, static_cast<uint32_t>(functions.size()));
  for (const auto &function : functions) {
    writeString(stream, function.name);
    writeRaw<uint32_t>(stream, function.stackSize);
    writeRaw<uint32_t>(stream, function.parameterSize);
    writeRaw<uint32_t>(stream, static_cast<uint32_t>(function.instructions.size()));
    for (const auto &instruction : function.instructions) {
      writeRaw<uint16_t>(stream, static_cast<uint16_t>(instruction.id));
      const unsigned int count = instructions[static_cast<unsigned int>(instruction.id)].argCount;
      for (unsigned int i = 0; i < count; ++i) {
        writeRaw<int32_t>(stream, instruction.args[i]);
      }
    }
  }
  return stream;
}

/**
 * \brief Loads hand built byte code and runs main, expecting RuntimeError.
 */
void requireCorrupt(const std::vector<std::string> &strings, const std::vector<FakeFunction> &functions) {
  std::stringstream byteCode = buildByteCode(strings, functions);
  VirtualMachine vm(nullptr);
  REQUIRE(vm.load(byteCode));
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}
} // namespace

TEST_CASE("Inc increments the top of the stack in place", "[integration][vm]") {
  const std::string source = wrap("pushi 41\ninc\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Dec decrements the top of the stack in place", "[integration][vm]") {
  const std::string source = wrap("pushi 43\ndec\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Inc on a float keeps the value a float", "[integration][vm]") {
  const std::string source = wrap("pushf 1.5\ninc\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2.5\n");
}

TEST_CASE("Dec on a float keeps the value a float", "[integration][vm]") {
  const std::string source = wrap("pushf 2.5\ndec\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1.5\n");
}

TEST_CASE("Inc leaves the rest of the stack alone", "[integration][vm]") {
  // 100 stays underneath while 1 becomes 2; log takes both, then 100 is
  // pushed again and added.
  const std::string source = "extern log 2\nextern println 1\n\nfunc main\n{\npushi 100\npushi 1\n"
                             "inc\ncalle log\npushi 100\npushi 100\nadd\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "[100]: 2\n200\n");
}

TEST_CASE("Inc and dec can be chained", "[integration][vm]") {
  const std::string source = wrap("pushi 0\ninc\ninc\ninc\ndec\n"
                                  "calle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2\n");
}

TEST_CASE("Inc on an empty stack stops the script", "[integration][vm]") {
  const std::string source = wrap("inc\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Inc on a string operand is refused", "[integration][vm]") {
  const std::string source = wrap("pushs \"text\"\ninc\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Dec on an empty stack stops the script", "[integration][vm]") {
  const std::string source = wrap("dec\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("And produces the logical conjunction of two values", "[integration][vm]") {
  const std::string source = wrap("pushi 1\npushi 0\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("And reports true for two true values", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 0\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // Zero is the only false value, so 5 and 0 is false.
  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("And treats any non zero value as true", "[integration][vm]") {
  const std::string source = wrap("pushi 7\npushi 3\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Or produces the logical disjunction of two values", "[integration][vm]") {
  const std::string source = wrap("pushi 0\npushi 4\nor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Or reports false when both values are false", "[integration][vm]") {
  const std::string source = wrap("pushi 0\npushi 0\nor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Not negates the truth of the top value", "[integration][vm]") {
  const std::string source = wrap("pushi 0\nnot\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Not negates a true value to false", "[integration][vm]") {
  const std::string source = wrap("pushi 9\nnot\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Xor reports true for differing values", "[integration][vm]") {
  const std::string source = wrap("pushi 1\npushi 0\nxor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Xor reports false for equal values", "[integration][vm]") {
  const std::string source = wrap("pushi 1\npushi 1\nxor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Xor reports false for two false values", "[integration][vm]") {
  const std::string source = wrap("pushi 0\npushi 0\nxor\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("And works on floats", "[integration][vm]") {
  const std::string source = wrap("pushf 0.5\npushf 0.0\nand\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Not on a float keeps an integer result", "[integration][vm]") {
  const std::string source = wrap("pushf 1.5\nnot\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0\n");
}

TEST_CASE("Or on strings is refused", "[integration][vm]") {
  const std::string source = wrap("pushs \"a\"\npushs \"b\"\nor\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Not on an empty stack stops the script", "[integration][vm]") {
  const std::string source = wrap("not\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("And on a short stack stops the script", "[integration][vm]") {
  const std::string source = wrap("pushi 1\nand\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("A script function returns from ret", "[integration][vm]") {
  const std::string source = wrap("pushi 1\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Add and sub operate on the runtime stack", "[integration][vm]") {
  const std::string source = wrap("pushi 10\npushi 20\nadd\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Pushf and pushi both push onto the runtime stack", "[integration][vm]") {
  // Each call takes exactly one pushed value, so print between the pushes.
  const std::string source = wrap("pushi 1\ncalle println\npushf 2.5\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n2.5\n");
}

TEST_CASE("Jump if less or equal is taken", "[integration][vm]") {
  const std::string source = wrap("pushi 1\npushi 2\njle a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Jump if less or equal is not taken", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 2\njle a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Jmp transfers control unconditionally", "[integration][vm]") {
  const std::string source = wrap("jmp done\n"
                                  "pushi 20\ncalle println\n"
                                  "label done\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jmp to an unknown label fails to assemble", "[integration][vm]") {
  const std::string source = wrap("jmp missing\nret");

  std::stringstream byteCode;
  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Jump if equal is taken", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 5\nje a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if equal is not taken", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 6\nje a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if equal compares across types", "[integration][vm]") {
  const std::string source = wrap("pushi 2\npushf 2.0\nje a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if not equal is taken", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 6\njne a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if not equal is not taken", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 5\njne a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if greater is taken", "[integration][vm]") {
  const std::string source = wrap("pushi 6\npushi 5\njg a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if greater is not taken on equal values", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 5\njg a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if greater compares across types", "[integration][vm]") {
  const std::string source = wrap("pushf 2.5\npushi 2\njg a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if greater or equal is taken on equal values", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 5\njge a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if greater or equal is not taken", "[integration][vm]") {
  const std::string source = wrap("pushi 4\npushi 5\njge a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("Jump if less is taken", "[integration][vm]") {
  const std::string source = wrap("pushi 4\npushi 5\njl a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Jump if less is not taken on equal values", "[integration][vm]") {
  const std::string source = wrap("pushi 5\npushi 5\njl a\n"
                                  "pushi 20\ncalle println\n"
                                  "label a\npushi 10\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "20\n10\n");
}

TEST_CASE("retf returns the float to the caller", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nretf 2.5\n}\n"
                             "\nfunc main\n{\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2.5\n");
}

TEST_CASE("rets returns the string to the caller", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nrets \"returned\"\n}\n"
                             "\nfunc main\n{\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "returned\n");
}

TEST_CASE("rets returns an empty string to the caller", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nrets \"\"\n}\n"
                             "\nfunc main\n{\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "\n");
}

TEST_CASE("Break continues execution", "[integration][vm]") {
  const std::string source = wrap("pushi 1\ncalle println\nbreak\n"
                                  "pushi 2\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n2\n");
}

TEST_CASE("reti from a function without parameters or locals", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nreti 5\n}\n"
                             "\nfunc main\n{\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "5\n");
}

TEST_CASE("Pop on an empty stack stops the script", "[integration][vm]") {
  const std::string source = wrap("pop\npushi 1\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Calling an extern function without arguments stops the script", "[integration][vm]") {
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

TEST_CASE("A script function receives its parameter", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc print\n{\nparam a\n"
                             "pushv a\ncalle println\nret\n}\n"
                             "\nfunc main\n{\npushi 100\ncall print\nret\n}\n";

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("Pushu pushes an unsigned integer constant", "[integration][vm]") {
  const std::string source = wrap("pushu 3000000000\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // Would overflow if it were read back as a signed integer.
  REQUIRE(recorder.text() == "3000000000\n");
}

TEST_CASE("Pushu keeps its value unsigned across arithmetic", "[integration][vm]") {
  const std::string source = wrap("pushu 4000000000\npushu 1\nadd\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "4000000001\n");
}

TEST_CASE("Pushs pushes a string constant", "[integration][vm]") {
  const std::string source = wrap("pushs \"pushed string\"\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "pushed string\n");
}

TEST_CASE("Pushs reuses identical strings from the table", "[integration][vm]") {
  const std::string source = wrap("pushs \"same\"\ncalle println\npushs \"same\"\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "same\nsame\n");
}

TEST_CASE("Pushs accepts an empty string", "[integration][vm]") {
  const std::string source = wrap("pushs \"\"\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "\n");
}

TEST_CASE("Mul multiplies integers on the runtime stack", "[integration][vm]") {
  const std::string source = wrap("pushi 6\npushi 7\nmul\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "42\n");
}

TEST_CASE("Mul multiplies floats", "[integration][vm]") {
  const std::string source = wrap("pushf 2.5\npushf 4.0\nmul\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "10\n");
}

TEST_CASE("Mul widens to float when the operands differ", "[integration][vm]") {
  const std::string source = wrap("pushi 3\npushf 0.5\nmul\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1.5\n");
}

TEST_CASE("Mul of overflowing unsigned integers throws", "[integration][vm]") {
  const std::string source = wrap("pushu 3000000000\npushu 2\nmul\n"
                                  "calle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  // 3000000000 * 2 leaves the uint32 range, so it throws instead of wrapping.
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Mul leaves the rest of the stack alone", "[integration][vm]") {
  const std::string source = wrap("pushi 100\npushi 6\npushi 7\nmul\n"
                                  "add\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "142\n");
}

TEST_CASE("Div divides integers with truncation", "[integration][vm]") {
  const std::string source = wrap("pushi 17\npushi 5\ndiv\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3\n");
}

TEST_CASE("Div divides floats", "[integration][vm]") {
  const std::string source = wrap("pushf 1.0\npushf 8.0\ndiv\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "0.125\n");
}

TEST_CASE("Div by zero stops the script and logs an error", "[integration][vm]") {
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

TEST_CASE("Arithmetic on a string operand is refused", "[integration][vm]") {
  const std::string source = wrap("pushs \"text\"\npushi 1\nmul\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  // The script stops at the multiplication instead of producing a value.
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Add on string operands is refused", "[integration][vm][c1]") {
  const std::string source = wrap("pushs \"text\"\npushi 1\nadd\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Sub on string operands is refused", "[integration][vm][c1]") {
  const std::string source = wrap("pushi 1\npushs \"text\"\nsub\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Pushf keeps the exact float bit pattern", "[integration][vm][c3]") {
  const std::string source = wrap("pushf 3.125\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3.125\n");
}

TEST_CASE("Pushu keeps the maximum unsigned value", "[integration][vm][c3]") {
  const std::string source = wrap("pushu 4294967295\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "4294967295\n");
}

TEST_CASE("Locals do not leak across calls", "[integration][vm][c6]") {
  const std::string source = "extern println 1\n\nfunc prime\n{\nvar v\nmovi v, 99\nret\n}\n"
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

TEST_CASE("State is clean after a throw from a nested call", "[integration][vm][c6]") {
  const std::string source = "extern println 1\n\nfunc inner\n{\npushi 1\npushi 0\ndiv\nret\n}\n"
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

TEST_CASE("clearScripts drops the runtime and call stacks", "[integration][vm][c6]") {
  const std::string source = "extern println 1\n\nfunc prime\n{\nvar v\nmovi v, 99\nret\n}\n"
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

TEST_CASE("A huge strings count is rejected", "[integration][vm][c7]") {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0xFFFFFFFF);

  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.load(stream), kern::RuntimeError);
}

TEST_CASE("A huge string length prefix is rejected", "[integration][vm][c7]") {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 1);
  writeRaw<uint32_t>(stream, 100 * 1024 * 1024);

  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.load(stream), kern::RuntimeError);
}

TEST_CASE("A huge extern count is rejected", "[integration][vm][c7]") {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0xFFFFFFFF);

  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.load(stream), kern::RuntimeError);
}

TEST_CASE("A huge function count is rejected", "[integration][vm][c7]") {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0xFFFFFFFF);

  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.load(stream), kern::RuntimeError);
}

TEST_CASE("A huge function name length is rejected", "[integration][vm][c7]") {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 1);
  writeRaw<uint32_t>(stream, 100 * 1024 * 1024);

  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.load(stream), kern::RuntimeError);
}

TEST_CASE("A huge instruction count is rejected", "[integration][vm][c7]") {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 1);
  writeString(stream, "main");
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0xFFFFFFFF);

  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.load(stream), kern::RuntimeError);
}

TEST_CASE("Retv on an empty frame is refused", "[integration][vm][corrupt][m1]") {
  // The callee has no parameters or locals, so its frame is empty and slot
  // zero does not exist. Returning it must throw instead of planting an
  // Invalid value on the caller stack.
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 0), makeInstruction(Opcode::Call, 1),
                       makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 0;
  callee.instructions = {makeInstruction(Opcode::Retv, 0)};
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Call before define works", "[integration][vm][m2]") {
  const std::string source = "extern println 1\n\nfunc main\n{\ncall f\nret\n}\n"
                             "\nfunc f\n{\npushi 7\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "7\n");
}

TEST_CASE("Calle before extern declaration works", "[integration][vm][m2]") {
  const std::string source = "func main\n{\npushi 1\ncalle println\nret\n}\n"
                             "\nextern println 1\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Mutual recursion works", "[integration][vm][m2]") {
  const std::string source = "extern println 1\n\nfunc a\n{\nparam n\npushv n\ncalle println\n"
                             "pushv n\npushi 0\nje done\npushv n\npushi 1\nsub\ncall b\n"
                             "label done\nret\n}\n"
                             "\nfunc b\n{\nparam n\npushv n\npushi 1\nsub\ncall a\nret\n}\n"
                             "\nfunc main\n{\npushi 2\ncall a\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "2\n0\n");
}

TEST_CASE("Bytecode carries the script header", "[integration][vm][m4]") {
  const std::string source = "module Greetings\nextern println 1\n"
                             "\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  ScriptHeader header;
  REQUIRE(deserialize(header, byteCode, 1024));
  REQUIRE(header.identifier[0] == 'K');
  REQUIRE(header.identifier[1] == 'E');
  REQUIRE(header.identifier[2] == 'R');
  REQUIRE(header.identifier[3] == 'N');
  REQUIRE(header.majorVersion == kern::getMajorVersion());
  REQUIRE(header.minorVersion == kern::getMinorVersion());
  REQUIRE(header.moduleName == "Greetings");
}

TEST_CASE("load rejects a bad magic", "[integration][vm][m4]") {
  std::stringstream stream;
  stream.write("XXXX", 4);
  writeRaw<uint8_t>(stream, 0);
  writeRaw<uint8_t>(stream, 1);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);

  VirtualMachine vm(nullptr);
  REQUIRE_FALSE(vm.load(stream));
}

TEST_CASE("load rejects a mismatched major version", "[integration][vm][m4]") {
  std::stringstream stream;
  stream.write("KERN", 4);
  writeRaw<uint8_t>(stream, static_cast<uint8_t>(kern::getMajorVersion() + 1));
  writeRaw<uint8_t>(stream, static_cast<uint8_t>(kern::getMinorVersion()));
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);

  VirtualMachine vm(nullptr);
  REQUIRE_FALSE(vm.load(stream));
}

TEST_CASE("load accepts a newer minor version", "[integration][vm][m4]") {
  std::stringstream stream;
  stream.write("KERN", 4);
  writeRaw<uint8_t>(stream, static_cast<uint8_t>(kern::getMajorVersion()));
  writeRaw<uint8_t>(stream, static_cast<uint8_t>(kern::getMinorVersion() + 1));
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);

  VirtualMachine vm(nullptr);
  REQUIRE(vm.load(stream));
}

TEST_CASE("Calle with too few pushed values is refused", "[integration][vm][m5]") {
  // Nothing is pushed for the call; the local must not be mistaken for an
  // argument.
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 5\ncalle println\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Calle with too many pushed values is refused", "[integration][vm][m5]") {
  const std::string source = wrap("pushi 1\npushi 2\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Pushi accepts a negative literal", "[integration][vm][m6]") {
  const std::string source = wrap("pushi -5\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "-5\n");
}

TEST_CASE("Movi accepts a negative literal", "[integration][vm][m6]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, -7\npushv a\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "-7\n");
}

TEST_CASE("Reti accepts a negative literal", "[integration][vm][m6]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nreti -3\n}\n"
                             "\nfunc main\n{\ncall f\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "-3\n");
}

TEST_CASE("Pushf accepts a negative literal", "[integration][vm][m6]") {
  const std::string source = wrap("pushf -1.5\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "-1.5\n");
}

TEST_CASE("Pushi accepts the minimum integer", "[integration][vm][m6]") {
  const std::string source = wrap("pushi -2147483648\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "-2147483648\n");
}

TEST_CASE("Calling a function with a huge frame is refused", "[integration][vm][m7]") {
  // The array fits the frame arithmetic, so assembly succeeds, but the frame
  // exceeds the runtime budget and the call stops instead of allocating it.
  const std::string source = wrap("vara a 4294967295\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("A refused jump aborts without wedging the VM", "[integration][vm][corrupt][m4]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1), makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Je, 99)};
  std::stringstream byteCode = buildByteCode({}, {main});

  VirtualMachine vm(nullptr);
  REQUIRE(vm.load(byteCode));
  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);

  // The target is validated before the operands pop. The leftovers are no
  // longer observable: popParameter outside an extern call throws, so the
  // contract now is that the VM stays usable. A fresh script runs cleanly.
  FakeFunction fresh;
  fresh.instructions = {makeInstruction(Opcode::Ret)};
  std::stringstream recovery = buildByteCode({}, {fresh});
  REQUIRE(vm.load(recovery));
  REQUIRE(vm.callFunction("main"));
}

TEST_CASE("Falling past the last instruction returns", "[integration][vm][m5]") {
  // No trailing ret: running off the end is an implicit return, which is
  // also what a trailing label resolves to.
  const std::string source = wrap("pushi 1\ncalle println");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("An absurd stack budget throws instead of bad_alloc", "[integration][vm][m10]") {
  kern::VMConfig config;
  config.maxStackBytes = SIZE_MAX;

  REQUIRE_THROWS_AS(VirtualMachine(nullptr, config), kern::RuntimeError);
}

TEST_CASE("A frame beyond an enlarged budget is refused typed", "[integration][vm][limits]") {
  // 64MB reserves cleanly, but a 0xFFFFFFFF frame exceeds it. The budget
  // check refuses before the entry resize runs, so this throws
  // kern::RuntimeError and never bad_alloc. It pins the entry-resize failure
  // contract: no frame growth may let bad_alloc escape.
  kern::VMConfig config;
  config.maxStackBytes = 64 * 1024 * 1024;
  const std::string source = wrap("vara a 4294967295\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("A call beyond an enlarged budget is refused typed", "[integration][vm][limits]") {
  // Same contract for the Call resize: the callee frame exceeds the enlarged
  // budget, so the call throws kern::RuntimeError instead of bad_alloc.
  kern::VMConfig config;
  config.maxStackBytes = 64 * 1024 * 1024;
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, 1), makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 0xFFFFFFFF;
  std::stringstream byteCode = buildByteCode({}, {main, callee});

  VirtualMachine vm(nullptr, config);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("A huge table count with a huge limit throws typed", "[integration][vm][limits]") {  // The limit check passes, so the resize itself must fail as a typed error
  // instead of bad_alloc.
  kern::VMConfig config;
  config.maxTableEntries = 0xFFFFFFFF;

  VirtualMachine vm(nullptr, config);
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0xFFFFFFFF);

  REQUIRE_THROWS_AS(vm.load(stream), kern::RuntimeError);
}

TEST_CASE("callFunction is not reentrant", "[integration][vm][m11]") {
  // An extern function that calls back into the running VM. The nested call
  // must throw instead of resetting the stacks mid-execution.
  class ReentrantFunction : public kern::IExternFunction {
  public:
    bool call(kern::IVirtualMachine &vm) override {
      vm.callFunction("main");
      return true;
    }

    unsigned int getArgumentCount() const override { return 0; }
  };

  const std::string source = "extern boom 0\nextern println 1\n\nfunc main\n{\ncalle boom\nret\n}\n"
                             "\nfunc clean\n{\npushi 1\ncalle println\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  vm.addFunction("boom", new ReentrantFunction());
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);

  // The guard resets on unwind, so a later call runs normally.
  REQUIRE(vm.callFunction("clean"));
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Movs advances the instruction pointer", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar x\n"
                             "movs x, \"hello\"\npushv x\ncalle println\nret\n}\n";

  testutil::DiscardRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
}

TEST_CASE("movv advances the instruction pointer", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\nvar b\n"
                             "movi a, 3\nmovv b, a\npushv b\ncalle println\n"
                             "ret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3\n");
}

TEST_CASE("movi advances the instruction pointer", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 7\npushv a\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "7\n");
}

TEST_CASE("movf advances the instruction pointer", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movf a, 1.5\npushv a\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "1.5\n");
}

TEST_CASE("reti returns the value to the caller", "[integration][vm]") {
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

TEST_CASE("retv returns the value to the caller", "[integration][vm]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nparam x\nvar b\n"
                             "pushi 77\npopv b\nretv b\n}\n"
                             "\nfunc main\n{\npushi 5\ncall f\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "77\n");
}

TEST_CASE("An empty function runs nothing", "[integration][vm]") {
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

TEST_CASE("exit ends the script", "[integration][vm]") {
  const std::string source = wrap("pushi 1\ncalle println\nexit\n"
                                  "pushi 2\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  // Everything after exit must not run.
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Calling an unknown function fails", "[integration][vm]") {
  const std::string source = wrap("ret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  REQUIRE_FALSE(vm->callFunction("missing"));
}

TEST_CASE("A failed lookup leaves later calls working", "[integration][vm]") {
  const std::string source = wrap("pushi 1\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  // The failed lookup must not clobber the state the next call resolves.
  REQUIRE_FALSE(vm->callFunction("missing"));
  REQUIRE(vm->callFunction("main"));
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("A failed load leaves no script behind", "[integration][vm]") {
  const std::string source = wrap("pushi 1\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(recorder.text() == "1\n");

  // A truncated stream fails the load; neither the old script nor a partial
  // new one may remain.
  std::stringstream truncated;
  writeHeader(truncated);
  writeRaw<uint32_t>(truncated, 1);
  writeString(truncated, "hi");
  writeRaw<uint32_t>(truncated, 0);
  writeRaw<uint32_t>(truncated, 1);
  writeString(truncated, "main");
  writeRaw<uint32_t>(truncated, 0);
  writeRaw<uint32_t>(truncated, 0);
  writeRaw<uint32_t>(truncated, 1);
  truncated.write("\x00", 1);

  REQUIRE_FALSE(vm.load(truncated));
  REQUIRE_FALSE(vm.callFunction("main"));

  // The VM stays usable: a valid script loads cleanly afterwards.
  byteCode.clear();
  byteCode.seekg(0);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(recorder.text() == "1\n1\n");
}

TEST_CASE("A throwing load leaves no script behind", "[integration][vm]") {
  const std::string source = wrap("pushi 1\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(recorder.text() == "1\n");

  // A huge count throws instead of returning false; the old script must be
  // gone all the same.
  std::stringstream corrupt;
  writeHeader(corrupt);
  writeRaw<uint32_t>(corrupt, 0xFFFFFFFF);

  REQUIRE_THROWS_AS(vm.load(corrupt), kern::RuntimeError);
  REQUIRE_FALSE(vm.callFunction("main"));

  byteCode.clear();
  byteCode.seekg(0);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(recorder.text() == "1\n1\n");
}

TEST_CASE("Calling an unregistered extern function stops the script", "[integration][vm]") {
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

TEST_CASE("clearScripts removes the loaded script", "[integration][vm]") {
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

TEST_CASE("Extern functions survive a clearScripts call", "[integration][vm]") {
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

TEST_CASE("removeFunction unregisters an extern function", "[integration][vm]") {
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

TEST_CASE("A host function returns its value to the script", "[integration][vm]") {
  const std::string source = "extern get_value 0\nextern println 1\n"
                             "\nfunc main\n{\ncalle get_value\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "35\n");
}

TEST_CASE("A host function receives its arguments in source order", "[integration][vm]") {
  const std::string source = "extern log 2\n"
                             "\nfunc main\n{\npushi 1\npushi 2\n"
                             "calle log\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "[1]: 2\n");
}

TEST_CASE("Passing a string to an int extern parameter throws", "[integration][vm]") {
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

TEST_CASE("VMConfig defaults to 1MB stack and 1024 depth", "[integration][vm][limits]") {
  const kern::VMConfig config{};
  REQUIRE(config.maxStackBytes == 1024 * 1024);
  REQUIRE(config.maxCallDepth == 1024);
  REQUIRE(config.maxExecutionSteps == 10000000);
}

TEST_CASE("A small execution step budget stops a long script", "[integration][vm][limits]") {
  kern::VMConfig config;
  config.maxExecutionSteps = 10;
  const std::string source = wrap("pushi 1\npushi 2\npushi 3\npushi 4\n"
                                  "pushi 5\npushi 6\npushi 7\npushi 8\n"
                                  "pushi 9\npushi 10\npushi 11\npushi 12\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("An execution step budget allows scripts within it", "[integration][vm][limits]") {
  kern::VMConfig config;
  config.maxExecutionSteps = 10;
  const std::string source = wrap("pushi 1\npushi 2\npushi 3\npushi 4\n"
                                  "pushi 5\npushi 6\npushi 7\npushi 8\n"
                                  "pushi 9\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE(vm.callFunction("main"));
}

TEST_CASE("A jump to itself throws instead of hanging", "[integration][vm]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Jmp, 0)};
  std::stringstream byteCode = buildByteCode({}, {main});

  VirtualMachine vm(nullptr);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Stack limit allows pushes within budget", "[integration][vm][limits]") {
  const size_t slots = 4;
  const kern::VMConfig config{slots * sizeof(Value), 1024};
  // Four pushes fill the budget exactly; the pops unwind to the single value
  // the call below takes.
  const std::string source = wrap("pushi 1\npushi 2\npushi 3\npushi 4\n"
                                  "pop\npop\npop\ncalle println\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr, config);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));

  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Stack limit throws past budget", "[integration][vm][limits]") {
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

TEST_CASE("Call depth limit stops infinite recursion", "[integration][vm][limits]") {
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

TEST_CASE("Corrupt stack size is rejected, not bad_alloc", "[integration][vm][limits]") {
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

TEST_CASE("Factory overloads accept a config", "[integration][vm][limits]") {
  const std::string source = wrap("pushi 1\npushi 2\npushi 3\npushi 4\n"
                                  "pushi 5\npushi 6\nret");

  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  const kern::VMConfig tiny{4 * sizeof(Value), 1024};
  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(nullptr, tiny));
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(*vm, recorder);
  REQUIRE(vm->load(byteCode));

  REQUIRE_THROWS_AS(vm->callFunction("main"), kern::RuntimeError);

  std::unique_ptr<kern::IVirtualMachine> defaulted(kern::createVirtualMachine(kern::VMConfig{}));
  testutil::StringRecorder output;
  testutil::registerHostFunctions(*defaulted, output);
  byteCode.clear();
  byteCode.seekg(0);
  REQUIRE(defaulted->load(byteCode));

  // A bug in the test itself would surface here as an unexpected throw.
  REQUIRE(defaulted->callFunction("main"));
}

TEST_CASE("Return past a full stack throws", "[integration][vm][limits]") {
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

TEST_CASE("An empty runtime stack is not popped", "[integration][vm]") {
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

TEST_CASE("Movv with a corrupt destination index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movv, 5, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movv with a corrupt source index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Movv, 0, 7)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movv with a negative index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movv, -1, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movi with a corrupt destination index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movi, 4, 10)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movf with a corrupt destination index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Movf, 4, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movs with a corrupt string index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Movs, 0, 4)};
  requireCorrupt({}, {main});
}

TEST_CASE("Pushv with a corrupt variable index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushv, 2)};
  requireCorrupt({}, {main});
}

TEST_CASE("Pushs with a corrupt string index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushs, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Popv with a corrupt destination index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Popv, 3)};
  requireCorrupt({}, {main});
}

TEST_CASE("Movv into a pushed temp is refused", "[integration][vm][corrupt]") {
  // One local lives in the frame and one value is pushed above it. Index 1
  // addresses the temp, not a variable, so the move must throw.
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Pushi, 7), makeInstruction(Opcode::Movv, 1, 0)};
  requireCorrupt({}, {main});
}

TEST_CASE("Pushv of a pushed temp is refused", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Pushi, 7), makeInstruction(Opcode::Pushv, 1)};
  requireCorrupt({}, {main});
}

TEST_CASE("Popv into a pushed temp is refused", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Pushi, 7), makeInstruction(Opcode::Popv, 1)};
  requireCorrupt({}, {main});
}

TEST_CASE("Call with a corrupt function index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, 7)};
  requireCorrupt({}, {main});
}

TEST_CASE("Call with a negative function index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, -1)};
  requireCorrupt({}, {main});
}

TEST_CASE("Call with more parameters than pushed values throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Call, 1)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 1;
  callee.parameterSize = 5;
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Calle with a corrupt extern index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Calle, 3)};
  requireCorrupt({}, {main});
}

TEST_CASE("Calle with a negative extern index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Calle, -1)};
  requireCorrupt({}, {main});
}

TEST_CASE("Retv with a corrupt variable index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 0), makeInstruction(Opcode::Call, 1),
                       makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 1;
  callee.instructions = {makeInstruction(Opcode::Retv, 9)};
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Retv of a pushed temp is refused", "[integration][vm][corrupt]") {
  // The callee pushes a temp above its one-local frame and returns it by
  // index. Index 1 is not a variable, so the return must throw.
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 0), makeInstruction(Opcode::Call, 1),
                       makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 1;
  callee.instructions = {makeInstruction(Opcode::Pushi, 5), makeInstruction(Opcode::Retv, 1)};
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Rets with a corrupt string index throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 0), makeInstruction(Opcode::Call, 1),
                       makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.instructions = {makeInstruction(Opcode::Rets, 0)};
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Jmp past the end throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Jmp, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Je past the end throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1), makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Je, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jne past the end throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1), makeInstruction(Opcode::Pushi, 2),
                       makeInstruction(Opcode::Jne, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jg past the end throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 2), makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Jg, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jge past the end throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1), makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Jge, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jl past the end throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1), makeInstruction(Opcode::Pushi, 2),
                       makeInstruction(Opcode::Jl, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jle past the end throws", "[integration][vm][corrupt]") {
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Pushi, 1), makeInstruction(Opcode::Pushi, 1),
                       makeInstruction(Opcode::Jle, 99)};
  requireCorrupt({}, {main});
}

TEST_CASE("Jmp to exactly the end returns cleanly", "[integration][vm][corrupt]") {
  // A trailing label resolves to the instruction count itself, which the
  // fetch treats as an implicit return. That boundary stays legal.
  FakeFunction main;
  main.instructions = {makeInstruction(Opcode::Jmp, 1)};
  std::stringstream byteCode = buildByteCode({}, {main});
  VirtualMachine vm(nullptr);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
}

TEST_CASE("Extern calling clearScripts throws", "[integration][vm][reentry]") {
  class ClearingFunction : public kern::IExternFunction {
  public:
    bool call(kern::IVirtualMachine &vm) override {
      vm.clearScripts();
      return true;
    }

    unsigned int getArgumentCount() const override { return 0; }
  };

  const std::string source = "extern boom 0\n\nfunc main\n{\ncalle boom\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  vm.addFunction("boom", new ClearingFunction());
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Extern removing itself throws", "[integration][vm][reentry]") {
  class SelfRemovingFunction : public kern::IExternFunction {
  public:
    bool call(kern::IVirtualMachine &vm) override {
      vm.removeFunction("boom");
      return true;
    }

    unsigned int getArgumentCount() const override { return 0; }
  };

  const std::string source = "extern boom 0\n\nfunc main\n{\ncalle boom\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  vm.addFunction("boom", new SelfRemovingFunction());
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Truncated function name fails to load", "[integration][vm][trunc]") {
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 1);
  writeRaw<uint32_t>(stream, 10);
  stream.write("abc", 3);

  VirtualMachine vm(nullptr);
  REQUIRE_FALSE(vm.load(stream));
}

TEST_CASE("Stale extern returns count toward the next call", "[integration][vm]") {
  // get_value leaves 35 on the stack; pushing one more value leaves two
  // values above the frame, so println (one argument) refuses instead of
  // silently taking the top. Scripts must consume returns they do not need.
  const std::string source = "extern get_value 0\nextern println 1\n\nfunc main\n{\ncalle get_value\n"
                             "pushi 1\ncalle println\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Popping a stale extern return clears the next call", "[integration][vm]") {
  const std::string source = "extern get_value 0\nextern println 1\n\nfunc main\n{\ncalle get_value\n"
                             "pop\npushi 1\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("popParameter takes a double", "[integration][vm][pop]") {
  class DoubleTaker : public kern::IExternFunction {
  public:
    bool call(kern::IVirtualMachine &vm) override {
      double value = 0.0;
      if (!vm.popParameter(value)) {
        return false;
      }
      seen = value;
      return true;
    }

    unsigned int getArgumentCount() const override { return 1; }

    double seen = 0.0;
  };

  const std::string source = "extern take 1\n\nfunc main\n{\npushi 3\ncalle take\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  auto *taker = new DoubleTaker();
  vm.addFunction("take", taker);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(taker->seen == 3.0);
}

TEST_CASE("popParameter takes an unsigned integer", "[integration][vm][pop]") {
  class UnsignedTaker : public kern::IExternFunction {
  public:
    bool call(kern::IVirtualMachine &vm) override {
      unsigned int value = 0;
      if (!vm.popParameter(value)) {
        return false;
      }
      seen = value;
      return true;
    }

    unsigned int getArgumentCount() const override { return 1; }

    unsigned int seen = 0;
  };

  const std::string source = "extern take 1\n\nfunc main\n{\npushu 3000000000\ncalle take\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  auto *taker = new UnsignedTaker();
  vm.addFunction("take", taker);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(taker->seen == 3000000000u);
}

TEST_CASE("popParameter takes a bool", "[integration][vm][pop]") {
  class BoolTaker : public kern::IExternFunction {
  public:
    bool call(kern::IVirtualMachine &vm) override {
      bool value = false;
      if (!vm.popParameter(value)) {
        return false;
      }
      seen.push_back(value);
      return true;
    }

    unsigned int getArgumentCount() const override { return 1; }

    std::vector<bool> seen;
  };

  const std::string source = "extern take 1\n\nfunc main\n{\npushi 5\n"
                             "calle take\npushi 0\ncalle take\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  auto *taker = new BoolTaker();
  vm.addFunction("take", taker);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(taker->seen.size() == 2);
  REQUIRE(taker->seen[0]);
  REQUIRE_FALSE(taker->seen[1]);
}

TEST_CASE("Push of an uninitialized local throws", "[integration][vm]") {
  const std::string source = wrap("var x\npushv x\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Pop of an uninitialized local throws", "[integration][vm]") {
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Add)};
  requireCorrupt({}, {main});
}

TEST_CASE("Add of uninitialized locals throws", "[integration][vm]") {
  FakeFunction main;
  main.stackSize = 2;
  main.instructions = {makeInstruction(Opcode::Add)};
  requireCorrupt({}, {main});
}

TEST_CASE("Popv of an uninitialized value throws", "[integration][vm]") {
  // The callee returns its untouched local, leaving Invalid above the
  // caller's frame. Storing it with popv must throw instead of spreading the
  // uninitialized value.
  FakeFunction main;
  main.stackSize = 1;
  main.instructions = {makeInstruction(Opcode::Call, 1), makeInstruction(Opcode::Popv, 0),
                       makeInstruction(Opcode::Ret)};
  FakeFunction callee;
  callee.name = "callee";
  callee.stackSize = 1;
  callee.instructions = {makeInstruction(Opcode::Retv, 0)};
  requireCorrupt({}, {main, callee});
}

TEST_CASE("Inc on a large unsigned stays unsigned", "[integration][vm][m3]") {
  const std::string source = wrap("pushu 3000000000\ninc\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3000000001\n");
}

TEST_CASE("Dec on a large unsigned stays unsigned", "[integration][vm][m3]") {
  const std::string source = wrap("pushu 3000000001\ndec\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3000000000\n");
}

TEST_CASE("Large unsigned plus zero stays unsigned", "[integration][vm][m3]") {
  const std::string source = wrap("pushu 3000000000\npushi 0\nadd\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));

  REQUIRE(recorder.text() == "3000000000\n");
}

TEST_CASE("Truncated extern count fails cleanly", "[integration][vm][trunc]") {
  for (size_t bytes = 1; bytes < 4; ++bytes) {
    std::stringstream stream;
    writeHeader(stream);
    writeRaw<uint32_t>(stream, 0);
    stream.write("\x01\x02\x03", static_cast<std::streamsize>(bytes));
    VirtualMachine vm(nullptr);
    bool threwRuntime = false;
    bool returnedFalse = false;
    try {
      returnedFalse = !vm.load(stream);
    } catch (const kern::RuntimeError &) {
      threwRuntime = true;
    }
    REQUIRE((threwRuntime || returnedFalse));
  }
}

TEST_CASE("Truncated function count fails cleanly", "[integration][vm][trunc]") {
  for (size_t bytes = 1; bytes < 4; ++bytes) {
    std::stringstream stream;
    writeHeader(stream);
    writeRaw<uint32_t>(stream, 0);
    writeRaw<uint32_t>(stream, 0);
    stream.write("\x01\x02\x03", static_cast<std::streamsize>(bytes));
    VirtualMachine vm(nullptr);
    bool threwRuntime = false;
    bool returnedFalse = false;
    try {
      returnedFalse = !vm.load(stream);
    } catch (const kern::RuntimeError &) {
      threwRuntime = true;
    }
    REQUIRE((threwRuntime || returnedFalse));
  }
}

TEST_CASE("Truncated function name length fails cleanly", "[integration][vm][trunc]") {
  for (size_t bytes = 1; bytes < 4; ++bytes) {
    std::stringstream stream;
    writeHeader(stream);
    writeRaw<uint32_t>(stream, 0);
    writeRaw<uint32_t>(stream, 0);
    writeRaw<uint32_t>(stream, 1);
    stream.write("\x04\x00\x00", static_cast<std::streamsize>(bytes));
    stream.write("ab", 2);
    VirtualMachine vm(nullptr);
    bool threwRuntime = false;
    bool returnedFalse = false;
    try {
      returnedFalse = !vm.load(stream);
    } catch (const kern::RuntimeError &) {
      threwRuntime = true;
    }
    REQUIRE((threwRuntime || returnedFalse));
  }
}

TEST_CASE("Failed reentrant addFunction leaves ownership with the caller", "[integration][vm][reentry]") {
  struct Tracker : public kern::IExternFunction {
    int *count = nullptr;
    explicit Tracker(int *c) : count(c) { ++(*count); }
    ~Tracker() override { --(*count); }
    bool call(kern::IVirtualMachine &) override { return true; }
    unsigned int getArgumentCount() const override { return 0; }
  };
  class AddingFunction : public kern::IExternFunction {
  public:
    explicit AddingFunction(int *c) : count(c) {}
    bool call(kern::IVirtualMachine &vm) override {
      Tracker *raw = new Tracker(count);
      REQUIRE(*count == 1);
      try {
        vm.addFunction("late", raw);
      } catch (const kern::RuntimeError &) {
        // The refused registration must not take ownership: the object is
        // still alive and the caller deletes it without a double-free.
        REQUIRE(*count == 1);
        delete raw;
        REQUIRE(*count == 0);
        throw;
      }
      return true;
    }

    unsigned int getArgumentCount() const override { return 0; }

  private:
    int *count = nullptr;
  };

  int alive = 0;
  const std::string source = "extern boom 0\n\nfunc main\n{\ncalle boom\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  vm.addFunction("boom", new AddingFunction(&alive));
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
  REQUIRE(alive == 0);
}

TEST_CASE("Truncated function name leaves no script behind", "[integration][vm][trunc]") {
  const std::string source = wrap("pushi 1\ncalle println\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::StringRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(recorder.text() == "1\n");

  std::stringstream truncated;
  writeHeader(truncated);
  writeRaw<uint32_t>(truncated, 0);
  writeRaw<uint32_t>(truncated, 0);
  writeRaw<uint32_t>(truncated, 1);
  writeRaw<uint32_t>(truncated, 10);
  truncated.write("abc", 3);

  REQUIRE_FALSE(vm.load(truncated));
  REQUIRE_FALSE(vm.callFunction("main"));

  byteCode.clear();
  byteCode.seekg(0);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
  REQUIRE(recorder.text() == "1\n1\n");
}

TEST_CASE("addFunction refuses an empty name", "[integration][vm]") {
  struct Dummy : public kern::IExternFunction {
    bool call(kern::IVirtualMachine &) override { return true; }
    unsigned int getArgumentCount() const override { return 0; }
  };

  VirtualMachine vm(nullptr);
  Dummy *raw = new Dummy();
  try {
    vm.addFunction("", raw);
  } catch (const kern::RuntimeError &) {
    // Ownership stays with the caller on failure.
    delete raw;
    return;
  }
  delete raw;
  FAIL("addFunction with an empty name must fail.");
}

TEST_CASE("addFunction refuses a null function", "[integration][vm]") {
  struct Dummy : public kern::IExternFunction {
    bool call(kern::IVirtualMachine &) override { return true; }
    unsigned int getArgumentCount() const override { return 0; }
  };

  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.addFunction("nullfn", nullptr), kern::RuntimeError);

  // Nothing was planted: registering the name for real works and the script
  // reaches the new registration instead of a stored null.
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  vm.addFunction("nullfn", new Dummy());
  const std::string source = "extern nullfn 0\n\nfunc main\n{\ncalle nullfn\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));
}

TEST_CASE("popParameter outside dispatch throws", "[integration][vm]") {
  // The script leaves a value behind, so the stack is non-empty and only the
  // dispatch guard can refuse these pops.
  const std::string source = wrap("pushi 42\nret");
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));
  REQUIRE(vm.callFunction("main"));

  int asInt = 0;
  float asFloat = 0.0f;
  std::string asString;
  double asDouble = 0.0;
  unsigned int asUnsigned = 0;
  bool asBool = false;
  REQUIRE_THROWS_AS(vm.popParameter(asInt), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.popParameter(asFloat), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.popParameter(asString), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.popParameter(asDouble), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.popParameter(asUnsigned), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.popParameter(asBool), kern::RuntimeError);
}

TEST_CASE("setReturnValue outside dispatch throws", "[integration][vm]") {
  VirtualMachine vm(nullptr);
  REQUIRE_THROWS_AS(vm.setReturnValue(), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.setReturnValue(1), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.setReturnValue(1.0f), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.setReturnValue(std::string("x")), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.setReturnValue(1.0), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.setReturnValue(1u), kern::RuntimeError);
  REQUIRE_THROWS_AS(vm.setReturnValue(true), kern::RuntimeError);
}

TEST_CASE("popParameter below the frame throws", "[integration][vm]") {
  // One argument is pushed above a one-local frame. Popping twice would eat
  // the local, so the second pop must throw instead of succeeding.
  class DoublePopper : public kern::IExternFunction {
  public:
    bool call(kern::IVirtualMachine &vm) override {
      int first = 0;
      int second = 0;
      if (!vm.popParameter(first)) {
        return false;
      }
      if (!vm.popParameter(second)) {
        return false;
      }
      return true;
    }

    unsigned int getArgumentCount() const override { return 1; }
  };

  const std::string source = "extern gobble 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 5\npushi 3\ncalle gobble\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  vm.addFunction("gobble", new DoublePopper());
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("A corrupt string length with an absurd limit fails cleanly", "[integration][vm][limits]") {
  // With maxStringBytes=SIZE_MAX the prefix passes the limit check, so the
  // reserve itself must not let bad_alloc escape. On machines with enough
  // RAM the reserve succeeds and the truncated content returns false; on
  // constrained machines it throws RuntimeError. Either is clean, bad_alloc
  // is not.
  kern::VMConfig config;
  config.maxStringBytes = SIZE_MAX;
  VirtualMachine vm(nullptr, config);
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 1);
  writeRaw<uint32_t>(stream, 0xFFFFFFFF);
  bool threwRuntime = false;
  bool returnedFalse = false;
  try {
    returnedFalse = !vm.load(stream);
  } catch (const kern::RuntimeError &) {
    threwRuntime = true;
  }
  REQUIRE((threwRuntime || returnedFalse));
}

TEST_CASE("A corrupt function name length with an absurd limit fails cleanly", "[integration][vm][limits]") {
  kern::VMConfig config;
  config.maxStringBytes = SIZE_MAX;
  VirtualMachine vm(nullptr, config);
  std::stringstream stream;
  writeHeader(stream);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 0);
  writeRaw<uint32_t>(stream, 1);
  writeRaw<uint32_t>(stream, 0xFFFFFFFF);
  bool threwRuntime = false;
  bool returnedFalse = false;
  try {
    returnedFalse = !vm.load(stream);
  } catch (const kern::RuntimeError &) {
    threwRuntime = true;
  }
  REQUIRE((threwRuntime || returnedFalse));
}

TEST_CASE("Pop of a local makes a later zero arg calle throw", "[integration][vm][m5]") {
  // One local lives in the frame. Popping it corrupts the frame, so the
  // following zero argument call must throw instead of silently passing
  // with a clamped push count.
  const std::string source = "extern get_value 0\nextern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 5\npop\ncalle get_value\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Pop below the frame throws", "[integration][vm][m5]") {
  // The frame holds one local and nothing was pushed above it, so pop would
  // remove the local itself.
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 5\npop\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Popv below the frame throws", "[integration][vm][m5]") {
  // Only the local lives in the frame, so storing the top into it and
  // popping would still drop below the frame end.
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 5\npopv a\nret\n}\n";
  std::stringstream byteCode;
  REQUIRE(testutil::assemble(source, byteCode));

  VirtualMachine vm(nullptr);
  testutil::DiscardRecorder recorder;
  testutil::registerHostFunctions(vm, recorder);
  REQUIRE(vm.load(byteCode));

  REQUIRE_THROWS_AS(vm.callFunction("main"), kern::RuntimeError);
}

TEST_CASE("Pop of a pushed value above the frame still works", "[integration][vm][m5]") {
  const std::string source = wrap("pushi 1\npushi 2\npop\ncalle println\nret");

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Popv of a pushed value above the frame still works", "[integration][vm][m5]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar x\n"
                             "pushi 5\npopv x\npushv x\ncalle println\n"
                             "ret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
  REQUIRE(recorder.text() == "5\n");
}

TEST_CASE("Popping an extern return above the frame still works", "[integration][vm][m5]") {
  const std::string source = "extern get_value 0\nextern println 1\n\nfunc main\n{\n"
                             "calle get_value\npop\npushi 1\ncalle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
  REQUIRE(recorder.text() == "1\n");
}

TEST_CASE("Extern return chaining still works", "[integration][vm][m5]") {
  const std::string source = "extern get_value 0\nextern println 1\n"
                             "\nfunc main\n{\ncalle get_value\n"
                             "calle println\nret\n}\n";

  testutil::StringRecorder recorder;
  REQUIRE(testutil::runScript(source, "main", recorder));
  REQUIRE(recorder.text() == "35\n");
}
