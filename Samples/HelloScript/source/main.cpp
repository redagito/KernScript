#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <kern/Kern.h>

namespace
{

void println(std::string text) { std::cout << text << '\n'; }

} // namespace

int main()
{
    const std::string source = R"(extern println(x);
func main() {
  println("hello from script");
  println(40 + 2);
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
