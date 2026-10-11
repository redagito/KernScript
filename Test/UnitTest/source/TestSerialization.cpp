#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "common/Deserialize.h"
#include "common/ExternFunction.h"
#include "common/Instruction.h"
#include "common/Instructions.h"
#include "common/ScriptHeader.h"

#include <kern/RuntimeError.h>

namespace {

template <typename T> void writeRaw(std::stringstream &stream, T value) {
  stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
}
} // namespace

TEST_CASE("Instructions table matches the instruction enum", "[unit][serialize]") {
  REQUIRE(instructions[static_cast<unsigned int>(Opcode::Nop)].mnemonic == "nop");
  REQUIRE(instructions[static_cast<unsigned int>(Opcode::Jle)].mnemonic == "jle");
}

TEST_CASE("Every instruction has a mnemonic and an argument count", "[unit][serialize]") {
  const unsigned int count = static_cast<unsigned int>(sizeof(instructions) / sizeof(instructions[0]));

  for (unsigned int i = 0; i < count; ++i) {
    REQUIRE_FALSE(instructions[i].mnemonic.empty());
    REQUIRE(instructions[i].argCount <= 4);
  }
}

TEST_CASE("Instruction survives serialization", "[unit][serialize]") {
  Instruction instruction;
  instruction.id = Opcode::Movi;
  instruction.args[0] = 12;
  instruction.args[1] = 34;

  std::stringstream stream;
  REQUIRE(serialize(instruction, stream));

  Instruction result;
  REQUIRE(deserialize(result, stream));

  REQUIRE(result.id == instruction.id);
  REQUIRE(result.args[0] == instruction.args[0]);
  REQUIRE(result.args[1] == instruction.args[1]);
}

TEST_CASE("Instruction serializes only the declared arguments", "[unit][serialize]") {
  Instruction instruction;
  instruction.id = Opcode::Pushi;
  instruction.args[0] = 99;

  std::stringstream stream;
  REQUIRE(serialize(instruction, stream));

  const unsigned int expectedSize = sizeof(uint16_t) + sizeof(ArgumentType);
  REQUIRE(static_cast<unsigned int>(stream.str().size()) == expectedSize);
}

TEST_CASE("ExternFunction survives serialization", "[unit][serialize]") {
  ExternFunction externFunction;
  externFunction.name = "println";
  externFunction.argCount = 1;

  std::stringstream stream;
  REQUIRE(serialize(externFunction, stream));

  ExternFunction result;
  REQUIRE(deserialize(stream, result, 1024));

  REQUIRE(result.name == "println");
  REQUIRE(result.argCount == 1);
}

TEST_CASE("ScriptHeader survives serialization", "[unit][serialize]") {
  ScriptHeader header;
  header.identifier[0] = 'K';
  header.identifier[1] = 'E';
  header.identifier[2] = 'R';
  header.identifier[3] = 'N';
  header.majorVersion = 0;
  header.minorVersion = 1;
  header.moduleName = "Test";

  std::stringstream stream;
  REQUIRE(serialize(header, stream));

  ScriptHeader result;
  REQUIRE(deserialize(result, stream, 1024));

  REQUIRE(result.identifier[0] == 'K');
  REQUIRE(result.identifier[3] == 'N');
  REQUIRE(result.majorVersion == 0);
  REQUIRE(result.minorVersion == 1);
  REQUIRE(result.moduleName == "Test");
}

TEST_CASE("Deserialize reads integer types", "[unit][serialize]") {
  {
    std::stringstream stream;
    writeRaw<int8_t>(stream, -8);
    int8_t result = 0;
    REQUIRE(deserialize(stream, result));
    REQUIRE(result == -8);
  }
  {
    std::stringstream stream;
    writeRaw<uint8_t>(stream, 8);
    uint8_t result = 0;
    REQUIRE(deserialize(stream, result));
    REQUIRE(result == 8);
  }
  {
    std::stringstream stream;
    writeRaw<int16_t>(stream, -16);
    int16_t result = 0;
    REQUIRE(deserialize(stream, result));
    REQUIRE(result == -16);
  }
  {
    std::stringstream stream;
    writeRaw<uint16_t>(stream, 16);
    uint16_t result = 0;
    REQUIRE(deserialize(stream, result));
    REQUIRE(result == 16);
  }
  {
    std::stringstream stream;
    writeRaw<int32_t>(stream, -32);
    int32_t result = 0;
    REQUIRE(deserialize(stream, result));
    REQUIRE(result == -32);
  }
  {
    std::stringstream stream;
    writeRaw<uint32_t>(stream, 32);
    uint32_t result = 0;
    REQUIRE(deserialize(stream, result));
    REQUIRE(result == 32);
  }
  {
    std::stringstream stream;
    writeRaw<uint64_t>(stream, 64);
    uint64_t result = 0;
    REQUIRE(deserialize(stream, result));
    REQUIRE(result == 64);
  }
}

TEST_CASE("Deserialize reports failure on an empty stream", "[unit][serialize]") {
  std::stringstream stream;
  int32_t result = 0;
  REQUIRE_FALSE(deserialize(stream, result));
}

TEST_CASE("Deserialize reads strings", "[unit][serialize]") {
  const std::string text = "a string constant";

  std::stringstream stream;
  const uint32_t length = static_cast<uint32_t>(text.size());
  writeRaw<uint32_t>(stream, length);
  stream.write(text.data(), length);

  std::string result;
  REQUIRE(deserialize(stream, result, 1024));
  REQUIRE(result == text);
}

TEST_CASE("Deserialize reports failure on a truncated stream", "[unit][serialize]") {
  std::stringstream stream;
  const uint32_t length = 100;
  writeRaw<uint32_t>(stream, length);
  stream.write("short", 5);

  std::string result;
  REQUIRE_FALSE(deserialize(stream, result, 1024));
}

TEST_CASE("Deserialize leaves the target alone on a truncated string", "[unit][serialize][trunc]") {
  std::stringstream stream;
  writeRaw<uint32_t>(stream, 10);
  stream.write("abc", 3);

  std::string result = "sentinel";
  REQUIRE_FALSE(deserialize(stream, result, 1024));
  REQUIRE(result == "sentinel");
}

TEST_CASE("Deserialize throws on a string longer than the limit", "[unit][serialize][c7]") {
  std::stringstream stream;
  writeRaw<uint32_t>(stream, 16);
  stream.write("1234567890123456", 16);

  std::string result;
  REQUIRE_THROWS_AS(deserialize(stream, result, 15), kern::RuntimeError);
}

TEST_CASE("Deserialize accepts a string at exactly the limit", "[unit][serialize][c7]") {
  std::stringstream stream;
  writeRaw<uint32_t>(stream, 16);
  stream.write("1234567890123456", 16);

  std::string result;
  REQUIRE(deserialize(stream, result, 16));
  REQUIRE(result == "1234567890123456");
}

TEST_CASE("Deserialize clears the target on an empty string", "[unit][serialize][m1]") {
  std::stringstream stream;
  writeRaw<uint32_t>(stream, 0);

  std::string result = "stale";
  REQUIRE(deserialize(stream, result, 1024));
  REQUIRE(result.empty());
}

TEST_CASE("Default constructed Instruction has zero arguments", "[unit][serialize][m6]") {
  Instruction instruction;
  REQUIRE(instruction.id == Opcode::Nop);
  for (int i = 0; i < 4; ++i) {
    REQUIRE(instruction.args[i] == 0);
  }
}

TEST_CASE("Serialize refuses an unknown instruction id", "[unit][serialize]") {
  Instruction instruction;
  instruction.id = static_cast<Opcode>(999);

  std::stringstream stream;
  REQUIRE_FALSE(serialize(instruction, stream));
  REQUIRE(stream.str().empty());
}

TEST_CASE("Deserialize refuses an unknown instruction id", "[unit][serialize]") {
  std::stringstream stream;
  writeRaw<uint16_t>(stream, 999);

  Instruction result;
  REQUIRE_FALSE(deserialize(result, stream));
}

TEST_CASE("ExternFunction with an empty name is refused", "[unit][serialize][m12]") {
  ExternFunction empty;
  empty.argCount = 0;

  std::stringstream stream;
  REQUIRE_FALSE(serialize(empty, stream));

  std::stringstream corrupt;
  writeRaw<uint32_t>(corrupt, 0);
  writeRaw<uint8_t>(corrupt, 0);

  ExternFunction result;
  REQUIRE_FALSE(deserialize(corrupt, result, 1024));
}

TEST_CASE("toString names an unknown instruction id", "[unit][serialize]") {
  Instruction instruction;
  instruction.id = static_cast<Opcode>(999);

  REQUIRE(toString(instruction) == "<invalid>");
}
