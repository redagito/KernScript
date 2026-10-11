#include <catch2/catch_test_macros.hpp>

#include "testutil/TestSupport.hpp"

namespace {

const std::string header = "extern println 1\nextern print 1\n";

std::string wrap(const std::string &body) { return header + "\nfunc main\n{\n" + body + "\n}\n"; }
} // namespace

TEST_CASE("Assembler rejects an overflowing pushi literal", "[integration][assembler][c4]") {
  const std::string source = wrap("pushi 9999999999\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing pushu literal", "[integration][assembler][c4]") {
  const std::string source = wrap("pushu 99999999999\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing movi literal", "[integration][assembler][c4]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movi a, 9999999999\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing reti literal", "[integration][assembler][c4]") {
  const std::string source = "extern println 1\n\nfunc f\n{\n"
                             "reti 9999999999\n}\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing vara size", "[integration][assembler][c4]") {
  const std::string source = wrap("vara a 99999999999999999999\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing extern argument count", "[integration][assembler][c4]") {
  const std::string source = "extern foo 99999999999\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an extern argument count above 255", "[integration][assembler][c5]") {
  const std::string source = "extern foo 256\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler accepts an extern argument count of 255", "[integration][assembler][c5]") {
  const std::string source = "extern foo 255\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a negative extern argument count", "[integration][assembler][c5]") {
  const std::string source = "extern foo -1\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate function", "[integration][assembler][m3]") {
  const std::string source = "func main\n{\nret\n}\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate extern function", "[integration][assembler][m3]") {
  const std::string source = "extern foo 1\nextern foo 1\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate parameter", "[integration][assembler][m3]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nparam a\nparam a\nret\n}\n"
                             "\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate variable", "[integration][assembler][m3]") {
  const std::string source = wrap("var a\nvar a\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate array variable", "[integration][assembler][m3]") {
  const std::string source = wrap("vara a 2\nvara a 3\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a variable shadowing a parameter", "[integration][assembler][m3]") {
  const std::string source = "extern println 1\n\nfunc f\n{\nparam a\nvar a\n"
                             "ret\n}\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a variable array overflowing the frame", "[integration][assembler][m7]") {
  const std::string source = wrap("vara a 4000000000\nvara b 400000000\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a variable after a huge array", "[integration][assembler][m7]") {
  // The array alone fills the frame; one more slot would wrap the frame size
  // back to zero.
  const std::string source = wrap("vara a 4294967295\nvar b\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an unresolved jump target", "[integration][assembler]") {
  const std::string source = wrap("pushi 1\njle missing\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an unknown variable", "[integration][assembler]") {
  const std::string source = wrap("pushi 1\npopv unknown\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a call to an unknown function", "[integration][assembler]") {
  const std::string source = wrap("call missing\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a call to an unknown extern function", "[integration][assembler]") {
  const std::string source = wrap("pushi 1\ncalle missing\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate label", "[integration][assembler]") {
  const std::string source = wrap("label a\nlabel a\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a zero sized array", "[integration][assembler]") {
  const std::string source = wrap("vara a 0\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects multiple module declarations", "[integration][assembler]") {
  const std::string source = "module First\nmodule Second\n\nfunc main\n{\n}\n";

  std::stringstream byteCode;
  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an invalid token", "[integration][assembler]") {
  const std::string source = wrap("invalidInstruction ret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a function without a name", "[integration][assembler]") {
  const std::string source = "func\n{\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an unknown standalone keyword", "[integration][assembler]") {
  const std::string source = "func main\n{\nret\n}\nbogus\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler parses a label and a jump", "[integration][assembler]") {
  const std::string source = wrap("pushi 1\njle end\npushi 2\ncalle println\n"
                                  "label end\nret");
  std::stringstream byteCode;

  REQUIRE(testutil::assemble(source, byteCode));
  REQUIRE_FALSE(byteCode.str().empty());
}

TEST_CASE("Assembler rejects an out-of-range movf literal", "[integration][assembler][float]") {
  const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                             "movf a, 99999999999999999999999999999999999999999.0\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an out-of-range pushf literal", "[integration][assembler][float]") {
  const std::string source = wrap("pushf 99999999999999999999999999999999999999999.0\nret");
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an out-of-range retf literal", "[integration][assembler][float]") {
  const std::string source = "extern println 1\n\nfunc f\n{\n"
                             "retf 99999999999999999999999999999999999999999.0\n"
                             "}\n\nfunc main\n{\nret\n}\n";
  std::stringstream byteCode;

  REQUIRE_FALSE(testutil::assemble(source, byteCode));
}
