#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "common/Deserialize.h"
#include "common/Instructions.h"
#include "common/Serialize.h"
#include "common/SExternFunction.h"
#include "common/SInstruction.h"
#include "common/SScriptHeader.h"

namespace
{

template <typename T> void writeRaw(std::stringstream &stream, T value)
{
  stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
}
}

TEST_CASE("Instructions table matches the instruction enum",
          "[unit][serialize]")
{
  REQUIRE(instructions[static_cast<unsigned int>(EInstruction::Nop)].mnemonic ==
          "nop");
  REQUIRE(instructions[static_cast<unsigned int>(EInstruction::Jle)].mnemonic ==
          "jle");
}

TEST_CASE("Every instruction has a mnemonic and an argument count",
          "[unit][serialize]")
{
  const unsigned int count =
      static_cast<unsigned int>(sizeof(instructions) / sizeof(instructions[0]));

  for (unsigned int i = 0; i < count; ++i)
  {
    REQUIRE_FALSE(instructions[i].mnemonic.empty());
    REQUIRE(instructions[i].argCount <= 4);
  }
}

TEST_CASE("SInstruction survives serialization", "[unit][serialize]")
{
  SInstruction instruction;
  instruction.id = EInstruction::Movi;
  instruction.args[0] = 12;
  instruction.args[1] = 34;

  std::stringstream stream;
  REQUIRE(serialize(instruction, stream));

  SInstruction result;
  REQUIRE(deserialize(result, stream));

  REQUIRE(result.id == instruction.id);
  REQUIRE(result.args[0] == instruction.args[0]);
  REQUIRE(result.args[1] == instruction.args[1]);
}

TEST_CASE("SInstruction serializes only the declared arguments",
          "[unit][serialize]")
{
  SInstruction instruction;
  instruction.id = EInstruction::Pushi;
  instruction.args[0] = 99;

  std::stringstream stream;
  REQUIRE(serialize(instruction, stream));

  const unsigned int expectedSize = sizeof(uint16_t) + sizeof(ArgumentType);
  REQUIRE(static_cast<unsigned int>(stream.str().size()) == expectedSize);
}

TEST_CASE("SExternFunction survives serialization", "[unit][serialize]")
{
  SExternFunction externFunction;
  externFunction.name = "println";
  externFunction.argCount = 1;

  std::stringstream stream;
  REQUIRE(serialize(externFunction, stream));

  SExternFunction result;
  REQUIRE(deserialize(stream, result));

  REQUIRE(result.name == "println");
  REQUIRE(result.argCount == 1);
}

TEST_CASE("SScriptHeader survives serialization", "[unit][serialize]")
{
  SScriptHeader header;
  header.identifier[0] = 'K';
  header.identifier[1] = 'E';
  header.identifier[2] = 'R';
  header.identifier[3] = 'N';
  header.majorVersion = 0;
  header.minorVersion = 1;
  header.moduleName = "Test";

  std::stringstream stream;
  REQUIRE(serialize(header, stream));

  SScriptHeader result;
  REQUIRE(deserialize(result, stream));

  REQUIRE(result.identifier[0] == 'K');
  REQUIRE(result.identifier[3] == 'N');
  REQUIRE(result.majorVersion == 0);
  REQUIRE(result.minorVersion == 1);
}

TEST_CASE("Deserialize reads integer types", "[unit][serialize]")
{
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

TEST_CASE("Deserialize reports failure on an empty stream",
          "[unit][serialize]")
{
  std::stringstream stream;
  int32_t result = 0;
  REQUIRE_FALSE(deserialize(stream, result));
}

TEST_CASE("Deserialize reads strings", "[unit][serialize]")
{
  const std::string text = "a string constant";

  std::stringstream stream;
  const uint32_t length = static_cast<uint32_t>(text.size());
  writeRaw<uint32_t>(stream, length);
  stream.write(text.data(), length);

  std::string result;
  REQUIRE(deserialize(stream, result));
  REQUIRE(result == text);
}

TEST_CASE("Deserialize reports failure on a truncated stream",
          "[unit][serialize]")
{
  std::stringstream stream;
  const uint32_t length = 100;
  writeRaw<uint32_t>(stream, length);
  stream.write("short", 5);

  std::string result;
  REQUIRE_FALSE(deserialize(stream, result));
}
