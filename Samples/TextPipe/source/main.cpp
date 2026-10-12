#include <cctype>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

namespace
{

std::vector<std::string> g_lines = {"info: started", "error: disk full", "info: ok", "error: timeout", "debug: x"};
std::size_t g_next = 0;

int has_line() { return g_next < g_lines.size() ? 1 : 0; }

std::string next_line()
{
    std::string line = g_lines[g_next];
    ++g_next;
    return line;
}

int contains(std::string line, std::string part) { return line.find(part) != std::string::npos ? 1 : 0; }

std::string upper(std::string line)
{
    for (char &c : line)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return line;
}

void println(std::string text) { std::cout << text << '\n'; }

} // namespace

int main()
{
    const std::string source = R"(extern has_line();
extern next_line();
extern contains(line, part);
extern upper(line);
extern println(x);
func main() {
  while (has_line() == 1) {
    var line = next_line();
    if (contains(line, "debug") == 0) {
      if (contains(line, "error") == 1) {
        var up = upper(line);
        println(up);
      } else {
        println(line);
      }
    }
  }
}
)";

    std::stringstream asmCode;
    {
        std::istringstream input(source);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode))
        {
            std::cerr << "compile failed\n";
            return 1;
        }
    }

    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed\n";
            return 1;
        }
    }

    try
    {
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
        vm->addFunction("has_line", kern::createExternFunction(&has_line));
        vm->addFunction("next_line", kern::createExternFunction(&next_line));
        vm->addFunction("contains", kern::createExternFunction(&contains));
        vm->addFunction("upper", kern::createExternFunction(&upper));
        vm->addFunction("println", kern::createExternFunction(&println));
        if (!vm->load(byteCode))
        {
            std::cerr << "load failed\n";
            return 1;
        }
        if (!vm->callFunction("main"))
        {
            std::cerr << "function main not found\n";
            return 1;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
