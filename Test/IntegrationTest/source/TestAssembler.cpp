#include "common/Program.h"
#include "testutil/TestSupport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
namespace
{
bool assemble(const std::string &text)
{
    std::stringstream out;
    return testutil::assemble(text, out);
}
std::string wrap(const std::string &body) { return "func main -> void {\n" + body + "\n}"; }
} // namespace
TEST_CASE("Assembly declarations require explicit return modes", "[integration][assembler]")
{
    for (const std::string &source :
         {"extern foo 0 func main -> void {ret}", "func main {ret}", "func main -> wrong {ret}"})
    {
        CAPTURE(source);
        REQUIRE_FALSE(assemble(source));
    }
    REQUIRE(assemble("extern foo 0 -> value func main -> void {ret}"));
}
TEST_CASE("Assembler catches integer and float overflow before encoding", "[integration][assembler]")
{
    for (const std::string &body :
         {"pushi 2147483648 pop ret", "pushi -2147483649 pop ret", "pushu 4294967296 pop ret", "pushu -1 pop ret",
          "pushf 99999999999999999999999999999999999999999.0 pop ret", "vara a 4294967296 ret"})
    {
        CAPTURE(body);
        REQUIRE_FALSE(assemble(wrap(body)));
    }
    REQUIRE(assemble(wrap("pushi -2147483648 pop pushu 4294967295 pop ret")));
}
TEST_CASE("Assembler verifies all symbolic references and duplicates", "[integration][assembler]")
{
    for (const std::string &body :
         {"call absent ret", "calle absent ret", "jmp absent ret", "var a var a ret", "param x param x ret",
          "var a param a ret", "label x label x ret", "pushv absent pop ret", "vara zero 0 ret"})
    {
        CAPTURE(body);
        REQUIRE_FALSE(assemble(wrap(body)));
    }
    REQUIRE_FALSE(assemble("func a -> void {ret} func a -> void {ret}"));
    REQUIRE_FALSE(assemble("extern a 0 -> void extern a 0 -> void func main -> void {ret}"));
    REQUIRE_FALSE(assemble("module a module b func main -> void {ret}"));
}
TEST_CASE("Assembler refuses malformed syntax instead of skipping unknown tokens", "[integration][assembler]")
{
    for (const std::string &source :
         {"unknown", "func main -> void {mystery ret}", "func main -> void {var a movi a 1 ret}",
          "func main -> void {pushs \"unterminated}", "func main -> void {"})
    {
        CAPTURE(source);
        REQUIRE_FALSE(assemble(source));
    }
}
TEST_CASE("Forward references and mutual recursive signatures link without execution", "[integration][assembler]")
{
    REQUIRE(assemble("func a -> void {call b ret} func b -> void {call a ret}"));
}
TEST_CASE("Quoted identifiers permit assembly keywords as names", "[integration][assembler]")
{
    REQUIRE(assemble("func \"call\" -> void {var \"param\" movi \"param\", 1 pushv \"param\" pop ret}"));
}
TEST_CASE("Assembly parameter declarations cannot be shifted by locals", "[integration][assembler]")
{
    const std::string source = "extern println 1 -> void func identity -> value { var before param x retv x } func "
                               "main -> void {pushi 7 call identity calle println ret}";
    testutil::StringRecorder recorder;
    REQUIRE(testutil::runScript(source, "main", recorder));
    REQUIRE(recorder.text() == "7\n");
}
TEST_CASE("Assembly stack joins loops and returns are verified before serialization", "[integration][assembler]")
{
    REQUIRE_FALSE(assemble(wrap("pushi 1 jmp loop label loop pushi 2 jmp loop")));
    REQUIRE_FALSE(assemble(wrap("pushi 1 ret")));
    REQUIRE_FALSE(assemble("func value -> value {ret}"));
    REQUIRE_FALSE(assemble("func value -> value {pushi 1}"));
    REQUIRE(assemble(wrap("label again pushi 1 pushi 2 je again ret")));
}
TEST_CASE("Assembler instances start with fresh state after success and failure", "[integration][assembler]")
{
    std::unique_ptr<kern::IAssembler> a(kern::createAssembler(nullptr));
    for (const std::string &text : {"func main -> void {ret}", "invalid", "func main -> void {ret}"})
    {
        std::stringstream in(text), out;
        REQUIRE(a->assemble(in, out) == (text != "invalid"));
    }
}
TEST_CASE("Assembler preserves module names and full signatures in portable bytecode", "[integration][assembler]")
{
    std::stringstream in("module demo extern value 2 -> value func main -> void {ret}"), out;
    std::unique_ptr<kern::IAssembler> a(kern::createAssembler(nullptr));
    REQUIRE(a->assemble(in, out));
    Program p;
    std::string error;
    REQUIRE(deserializeProgram(out, p, {}, error));
    REQUIRE(p.moduleName == "demo");
    REQUIRE(p.externs[0].signature == kern::FunctionSignature{2, kern::ReturnKind::Value});
}
