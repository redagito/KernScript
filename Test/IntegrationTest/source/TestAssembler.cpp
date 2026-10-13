#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

#include "testutil/TestSupport.hpp"

namespace
{

const std::string header = "extern println 1\nextern print 1\n";

std::string wrap(const std::string &body) { return header + "\nfunc main\n{\n" + body + "\n}\n"; }

std::vector<std::string> compileSource(const std::string &source, const std::string &sourceName, bool &ok)
{
    testutil::RecordingLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    std::stringstream input(source);
    std::stringstream asmCode;
    ok = compiler->compile(input, asmCode, sourceName);
    return logger.messages();
}

} // namespace

TEST_CASE("Assembler rejects an overflowing pushi literal", "[integration][assembler][c4]")
{
    const std::string source = wrap("pushi 9999999999\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing pushu literal", "[integration][assembler][c4]")
{
    const std::string source = wrap("pushu 99999999999\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing movi literal", "[integration][assembler][c4]")
{
    const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                               "movi a, 9999999999\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing reti literal", "[integration][assembler][c4]")
{
    const std::string source = "extern println 1\n\nfunc f\n{\n"
                               "reti 9999999999\n}\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing vara size", "[integration][assembler][c4]")
{
    const std::string source = wrap("vara a 99999999999999999999\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an overflowing extern argument count", "[integration][assembler][c4]")
{
    const std::string source = "extern foo 99999999999\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an extern argument count above 255", "[integration][assembler][c5]")
{
    const std::string source = "extern foo 256\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler accepts an extern argument count of 255", "[integration][assembler][c5]")
{
    const std::string source = "extern foo 255\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a negative extern argument count", "[integration][assembler][c5]")
{
    const std::string source = "extern foo -1\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate function", "[integration][assembler][m3]")
{
    const std::string source = "func main\n{\nret\n}\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate extern function", "[integration][assembler][m3]")
{
    const std::string source = "extern foo 1\nextern foo 1\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate parameter", "[integration][assembler][m3]")
{
    const std::string source = "extern println 1\n\nfunc f\n{\nparam a\nparam a\nret\n}\n"
                               "\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate variable", "[integration][assembler][m3]")
{
    const std::string source = wrap("var a\nvar a\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a duplicate array variable", "[integration][assembler][m3]")
{
    const std::string source = wrap("vara a 2\nvara a 3\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a variable shadowing a parameter", "[integration][assembler][m3]")
{
    const std::string source = "extern println 1\n\nfunc f\n{\nparam a\nvar a\n"
                               "ret\n}\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a variable array overflowing the frame", "[integration][assembler][m7]")
{
    const std::string source = wrap("vara a 4000000000\nvara b 400000000\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a variable after a huge array", "[integration][assembler][m7]")
{
    // The array alone fills the frame; one more slot would wrap the frame size
    // back to zero.
    const std::string source = wrap("vara a 4294967295\nvar b\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an unresolved jump target", "[integration][assembler]")
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

TEST_CASE("Assembler rejects a call to an unknown function", "[integration][assembler]")
{
    const std::string source = wrap("call missing\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a call to an unknown extern function", "[integration][assembler]")
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

TEST_CASE("Assembler rejects multiple module declarations", "[integration][assembler]")
{
    const std::string source = "module First\nmodule Second\n\nfunc main\n{\n}\n";

    std::stringstream byteCode;
    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an invalid token", "[integration][assembler]")
{
    const std::string source = wrap("invalidInstruction ret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a function without a name", "[integration][assembler]")
{
    const std::string source = "func\n{\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an unknown standalone keyword", "[integration][assembler]")
{
    const std::string source = "func main\n{\nret\n}\nbogus\n";
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

TEST_CASE("Assembler rejects an out-of-range movf literal", "[integration][assembler][float]")
{
    const std::string source = "extern println 1\n\nfunc main\n{\nvar a\n"
                               "movf a, 99999999999999999999999999999999999999999.0\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an out-of-range pushf literal", "[integration][assembler][float]")
{
    const std::string source = wrap("pushf 99999999999999999999999999999999999999999.0\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects an out-of-range retf literal", "[integration][assembler][float]")
{
    const std::string source = "extern println 1\n\nfunc f\n{\n"
                               "retf 99999999999999999999999999999999999999999.0\n"
                               "}\n\nfunc main\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Source rejects an at-sign in a func name", "[integration][compiler]")
{
    bool ok = true;
    const std::vector<std::string> messages = compileSource("func ma@in() {\n}\n", "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:1:6: error: Invalid character '@' in identifier.");
    REQUIRE(messages[1] == "note: use letters, digits and underscores only");
}

TEST_CASE("Source rejects an at-sign in a var name", "[integration][compiler]")
{
    bool ok = true;
    const std::vector<std::string> messages = compileSource("func main() {\nvar v@r;\n}\n", "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:2:5: error: Invalid character '@' in identifier.");
    REQUIRE(messages[1] == "note: use letters, digits and underscores only");
}

TEST_CASE("Source rejects an at-sign in a param name", "[integration][compiler]")
{
    bool ok = true;
    const std::vector<std::string> messages = compileSource("func main(p@1) {\n}\n", "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:1:11: error: Invalid character '@' in identifier.");
    REQUIRE(messages[1] == "note: use letters, digits and underscores only");
}

TEST_CASE("Source rejects an at-sign in an extern name", "[integration][compiler]")
{
    bool ok = true;
    const std::vector<std::string> messages = compileSource("extern ex@tern(x);\nfunc main() {\n}\n", "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:1:8: error: Invalid character '@' in identifier.");
    REQUIRE(messages[1] == "note: use letters, digits and underscores only");
}

TEST_CASE("Source rejects an at-sign in a call name", "[integration][compiler]")
{
    bool ok = true;
    const std::vector<std::string> messages = compileSource("func main() {\nfoo@bar();\n}\n", "test.ks", ok);
    REQUIRE_FALSE(ok);
    REQUIRE(messages.size() == 2);
    REQUIRE(messages[0] == "test.ks:2:1: error: Invalid character '@' in identifier.");
    REQUIRE(messages[1] == "note: use letters, digits and underscores only");
}

TEST_CASE("Source still accepts triple underscores without at-signs", "[integration][compiler]")
{
    const std::string source = "extern ext___999(x);\nfunc fo___123(p___1) {\n"
                               "  var v___1 = p___1;\n  ext___999(v___1);\n}\n"
                               "func main() {\n  fo___123(1);\n}\n";
    testutil::RecordingLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    std::stringstream input(source);
    std::stringstream asmCode;
    REQUIRE(compiler->compile(input, asmCode, "test.ks"));
    REQUIRE_FALSE(asmCode.str().empty());
}

TEST_CASE("Assembler round-trips an at-function name end-to-end on the VM", "[integration][assembler]")
{
    const std::string source = "extern println 1\n\nfunc math@sqrt\n{\npushi 42\ncalle println\nret\n}\n\n"
                               "func main\n{\ncall math@sqrt\nret\n}\n";
    std::stringstream byteCode;
    REQUIRE(testutil::assemble(source, byteCode));
    REQUIRE_FALSE(byteCode.str().empty());

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction("main"));
    REQUIRE(recorder.text() == "42\n");
    // String-keyed lookup of the mangled name itself, through serialization.
    REQUIRE(vm->callFunction("math@sqrt"));
    REQUIRE(recorder.text() == "42\n42\n");
}

TEST_CASE("Assembler rejects a duplicate at-function", "[integration][assembler]")
{
    const std::string source = "func math@sqrt\n{\nret\n}\nfunc math@sqrt\n{\nret\n}\n";
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler rejects a call to an unknown at-function", "[integration][assembler]")
{
    const std::string source = wrap("call other@fn\nret");
    std::stringstream byteCode;

    REQUIRE_FALSE(testutil::assemble(source, byteCode));
}

TEST_CASE("Assembler resolves an at-label jump end-to-end on the VM", "[integration][assembler]")
{
    const std::string source = wrap("jmp L@1\npushi 999\ncalle println\nlabel L@1\npushi 7\ncalle println\nret");
    std::stringstream byteCode;
    REQUIRE(testutil::assemble(source, byteCode));
    REQUIRE_FALSE(byteCode.str().empty());

    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    testutil::StringRecorder recorder;
    testutil::registerHostFunctions(*vm, recorder);
    REQUIRE(vm->load(byteCode));
    REQUIRE(vm->callFunction("main"));
    REQUIRE(recorder.text() == "7\n");
}
