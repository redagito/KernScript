#include <catch2/catch_test_macros.hpp>

#include "testutil/TestSupport.hpp"

namespace
{

const std::string header = "extern println 1\n";

std::string wrap(const std::string &body) { return header + "\nfunc main\n{\n" + body + "\n}\n"; }

void requireOutput(const std::string &source, const std::string &expected)
{
    testutil::StringRecorder recorder;
    REQUIRE(testutil::runScript(source, "main", recorder));
    REQUIRE(recorder.text() == expected);
}

} // namespace

TEST_CASE("e2e nop does nothing", "[e2e][instruction]")
{
    requireOutput(wrap("nop\npushi 1\ncalle println\nret"), "1\n");
}

TEST_CASE("e2e break continues execution", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 1\ncalle println\nbreak\n"
                       "pushi 2\ncalle println\nret"),
                  "1\n2\n");
}

TEST_CASE("e2e exit stops the script", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 1\ncalle println\nexit\n"
                       "pushi 2\ncalle println\nret"),
                  "1\n");
}

TEST_CASE("e2e movv copies between variables", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc main\n{\nvar a\nvar b\n"
                  "movi a, 3\nmovv b, a\npushv b\ncalle println\nret\n}\n",
                  "3\n");
}

TEST_CASE("e2e movi stores an integer", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc main\n{\nvar a\n"
                  "movi a, 7\npushv a\ncalle println\nret\n}\n",
                  "7\n");
}

TEST_CASE("e2e movf stores a float", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc main\n{\nvar a\n"
                  "movf a, 1.5\npushv a\ncalle println\nret\n}\n",
                  "1.5\n");
}

TEST_CASE("e2e movs stores a string", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc main\n{\nvar x\n"
                  "movs x, \"hello\"\npushv x\ncalle println\nret\n}\n",
                  "hello\n");
}

TEST_CASE("e2e pushv pushes a variable", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc main\n{\nvar a\n"
                  "movi a, 9\npushv a\ncalle println\nret\n}\n",
                  "9\n");
}

TEST_CASE("e2e pushi pushes an integer", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 42\ncalle println\nret"), "42\n");
}

TEST_CASE("e2e pushu pushes an unsigned integer", "[e2e][instruction]")
{
    requireOutput(wrap("pushu 3000000000\ncalle println\nret"), "3000000000\n");
}

TEST_CASE("e2e pushf pushes a float", "[e2e][instruction]")
{
    requireOutput(wrap("pushf 2.5\ncalle println\nret"), "2.5\n");
}

TEST_CASE("e2e pushs pushes a string", "[e2e][instruction]")
{
    requireOutput(wrap("pushs \"pushed\"\ncalle println\nret"), "pushed\n");
}

TEST_CASE("e2e pop discards the top value", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 1\npushi 2\npop\ncalle println\nret"), "1\n");
}

TEST_CASE("e2e popv stores the top value", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc main\n{\nvar x\n"
                  "pushi 5\npopv x\npushv x\ncalle println\nret\n}\n",
                  "5\n");
}

TEST_CASE("e2e call runs another function", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc f\n{\nreti 3\n}\n"
                  "\nfunc main\n{\ncall f\ncalle println\nret\n}\n",
                  "3\n");
}

TEST_CASE("e2e calle runs a host function", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 6\ncalle println\nret"), "6\n");
}

TEST_CASE("e2e ret ends the function", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 1\ncalle println\nret\n"
                       "pushi 2\ncalle println\n"),
                  "1\n");
}

TEST_CASE("e2e retv returns a variable", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc f\n{\nparam x\nvar b\n"
                  "pushi 77\npopv b\nretv b\n}\n"
                  "\nfunc main\n{\npushi 5\ncall f\ncalle println\nret\n}\n",
                  "77\n");
}

TEST_CASE("e2e reti returns an integer", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc a\n{\nparam x\nreti 20\n}\n"
                  "\nfunc main\n{\npushi 10\ncall a\ncalle println\nret\n}\n",
                  "20\n");
}

TEST_CASE("e2e retf returns a float", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc f\n{\nretf 2.5\n}\n"
                  "\nfunc main\n{\ncall f\ncalle println\nret\n}\n",
                  "2.5\n");
}

TEST_CASE("e2e rets returns a string", "[e2e][instruction]")
{
    requireOutput("extern println 1\nfunc f\n{\nrets \"returned\"\n}\n"
                  "\nfunc main\n{\ncall f\ncalle println\nret\n}\n",
                  "returned\n");
}

TEST_CASE("e2e add adds two integers", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 10\npushi 20\nadd\ncalle println\nret"), "30\n");
}

TEST_CASE("e2e sub subtracts two integers", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 10\npushi 20\nsub\ncalle println\nret"), "-10\n");
}

TEST_CASE("e2e mul multiplies two integers", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 6\npushi 7\nmul\ncalle println\nret"), "42\n");
}

TEST_CASE("e2e div divides two integers", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 20\npushi 4\ndiv\ncalle println\nret"), "5\n");
}

TEST_CASE("e2e inc increments the top value", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 41\ninc\ncalle println\nret"), "42\n");
}

TEST_CASE("e2e dec decrements the top value", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 43\ndec\ncalle println\nret"), "42\n");
}

TEST_CASE("e2e and conjoins two values", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 1\npushi 0\nand\ncalle println\n"
                       "pushi 7\npushi 3\nand\ncalle println\nret"),
                  "0\n1\n");
}

TEST_CASE("e2e or disjoins two values", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 0\npushi 0\nor\ncalle println\n"
                       "pushi 0\npushi 4\nor\ncalle println\nret"),
                  "0\n1\n");
}

TEST_CASE("e2e not negates one value", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 0\nnot\ncalle println\n"
                       "pushi 9\nnot\ncalle println\nret"),
                  "1\n0\n");
}

TEST_CASE("e2e xor exclusive-ors two values", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 1\npushi 0\nxor\ncalle println\n"
                       "pushi 1\npushi 1\nxor\ncalle println\nret"),
                  "1\n0\n");
}

TEST_CASE("e2e jmp jumps unconditionally", "[e2e][instruction]")
{
    requireOutput(wrap("jmp done\npushi 99\ncalle println\n"
                       "label done\npushi 10\ncalle println\nret"),
                  "10\n");
}

TEST_CASE("e2e je jumps on equality", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 5\npushi 5\nje a\npushi 99\ncalle println\n"
                       "label a\npushi 10\ncalle println\nret"),
                  "10\n");
}

TEST_CASE("e2e jne jumps on inequality", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 5\npushi 6\njne a\npushi 99\ncalle println\n"
                       "label a\npushi 10\ncalle println\nret"),
                  "10\n");
}

TEST_CASE("e2e jg jumps when greater", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 6\npushi 5\njg a\npushi 99\ncalle println\n"
                       "label a\npushi 10\ncalle println\nret"),
                  "10\n");
}

TEST_CASE("e2e jge jumps when greater or equal", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 5\npushi 5\njge a\npushi 99\ncalle println\n"
                       "label a\npushi 10\ncalle println\nret"),
                  "10\n");
}

TEST_CASE("e2e jl jumps when less", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 4\npushi 5\njl a\npushi 99\ncalle println\n"
                       "label a\npushi 10\ncalle println\nret"),
                  "10\n");
}

TEST_CASE("e2e jle jumps when less or equal", "[e2e][instruction]")
{
    requireOutput(wrap("pushi 1\npushi 2\njle a\npushi 99\ncalle println\n"
                       "label a\npushi 10\ncalle println\nret"),
                  "10\n");
}
