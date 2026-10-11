#include <fstream>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "testutil/TestSupport.hpp"

namespace
{

const std::string fixtureDirectory = "data/asm/";

std::string fixturePath(const std::string &name)
{
  return fixtureDirectory + name + ".kasm";
}

testutil::StringRecorder runFixture(const std::string &name)
{
  const std::string source = testutil::readFile(fixturePath(name));
  REQUIRE_FALSE(source.empty());

  testutil::StringRecorder recorder;
  testutil::runScript(source, "main", recorder);
  return recorder;
}
}

TEST_CASE("arithmetic chains mul div inc and dec", "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("arithmetic");

  REQUIRE(recorder.text() == "24\n3\n11\n9\n0\n");
}

TEST_CASE("logic evaluates truth tables and a combined expression",
          "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("logic");

  REQUIRE(recorder.text() == "0\n0\n0\n1\n"
                             "0\n1\n1\n1\n"
                             "0\n1\n1\n0\n"
                             "1\n0\n"
                             "0\n");
}

TEST_CASE("test1 prints its values and calls a two argument extern function",
          "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("test1");

  // log receives its arguments in source order, severity first. print emits
  // no trailing newline, so its output runs straight into the next println.
  REQUIRE(recorder.text() == "10\n200\n10000\n10123\n[1]: 2\n");
}

TEST_CASE("test2 prints floats", "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("test2");

  REQUIRE(recorder.text() == "10.2\n100\n0.0001\n");
}

TEST_CASE("test3 calls script functions repeatedly", "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("test3");

  REQUIRE(recorder.text() ==
          "1\n"
          "100\n200\n"
          "100\n200\n"
          "100\n200\n"
          "100\n200\n"
          "200\n");
}

TEST_CASE("test4 adds and subtracts on the stack", "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("test4");

  REQUIRE(recorder.text() == "30\n-10\n");
}

TEST_CASE("test5 takes the jump", "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("test5");

  // 1 <= 2 jumps over the block printing 20.
  REQUIRE(recorder.text() == "5\n10\n");
}

TEST_CASE("test6 returns a value from a called function", "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("test6");

  // The script prints the parameter 10, then the return value 20.
  REQUIRE(recorder.text() == "10\n20\n");
}

TEST_CASE("test7 moves strings into variables", "[e2e][fixture]")
{
  const testutil::StringRecorder recorder = runFixture("test7");

  REQUIRE(recorder.text() == "hello world!\n");
}

TEST_CASE("Byte code written to a file can be loaded back", "[e2e]")
{
  const std::string fixture = fixturePath("test3");
  const std::string byteCodePath = "asm_out/test_e2e.kbyte";

  {
    std::ifstream input(fixture);
    std::ofstream output(byteCodePath, std::ios::binary);
    REQUIRE(input.is_open());
    REQUIRE(output.is_open());

    std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
    REQUIRE(assembler->assemble(input, output));
  }

  testutil::StringRecorder recorder;

  {
    std::ifstream input(byteCodePath, std::ios::binary);
    REQUIRE(input.is_open());

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(input));
    vm->callFunction("main");
  }

  REQUIRE(recorder.text() ==
          "1\n"
          "100\n200\n"
          "100\n200\n"
          "100\n200\n"
          "100\n200\n"
          "200\n");
}

TEST_CASE("A missing fixture is reported as empty", "[e2e]")
{
  REQUIRE(testutil::readFile(fixturePath("no_such_fixture")).empty());
}

TEST_CASE("Two scripts do not leak state into each other", "[e2e]")
{
  runFixture("test1");
  runFixture("test4");

  const testutil::StringRecorder recorder = runFixture("test4");

  REQUIRE(recorder.text() == "30\n-10\n");
}
