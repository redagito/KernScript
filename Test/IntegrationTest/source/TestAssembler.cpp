#include <catch2/catch_test_macros.hpp>

#include "TestSupport.hpp"

namespace
{

const std::string header = "extern println 1\nextern print 1\n";

std::string wrap(const std::string &body)
{
  return header + "\nfunc main\n{\n" + body + "\n}\n";
}
}

TEST_CASE("Assembler rejects an unresolved jump target",
          "[integration][assembler]")
{
  const std::string source = wrap("pushi 1\njle missing\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an unknown variable", "[integration][assembler]")
{
  const std::string source = wrap("pushi 1\npopv unknown\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a call to an unknown function",
          "[integration][assembler]")
{
  const std::string source = wrap("call missing\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a call to an unknown extern function",
          "[integration][assembler]")
{
  const std::string source = wrap("pushi 1\ncalle missing\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate label", "[integration][assembler]")
{
  const std::string source = wrap("label a\nlabel a\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a zero sized array", "[integration][assembler]")
{
  const std::string source = wrap("vara a 0\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects multiple module declarations",
          "[integration][assembler]")
{
  const std::string source =
      "module First\nmodule Second\n\nfunc main\n{\n}\n";

  std::stringstream byteCode;
  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an invalid token", "[integration][assembler]")
{
  const std::string source = wrap("invalidInstruction ret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a function without a name",
          "[integration][assembler]")
{
  const std::string source = "func\n{\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler parses a label and a jump", "[integration][assembler]")
{
  const std::string source = wrap("pushi 1\njle end\npushi 2\ncalle println\n"
                                  "label end\nret");
  std::stringstream byteCode;

  REQUIRE(testutil::assemble(source, byteCode));
  REQUIRE_FALSE(byteCode.str().empty());
}
