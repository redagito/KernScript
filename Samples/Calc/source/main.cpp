#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

namespace
{

struct Job
{
    int a;
    std::string op;
    int b;
};

std::vector<Job> g_jobs = {{7, "+", 3}, {9, "-", 4}, {6, "*", 7}, {20, "/", 4}};
std::size_t g_next = 0;

int has_more() { return g_next < g_jobs.size() ? 1 : 0; }

int next_a() { return g_jobs[g_next].a; }

std::string next_op() { return g_jobs[g_next].op; }

int next_b()
{
    int b = g_jobs[g_next].b;
    ++g_next;
    return b;
}

void println(std::string text) { std::cout << text << '\n'; }

} // namespace

int main()
{
    const std::string source = R"(extern has_more();
extern next_a();
extern next_op();
extern next_b();
extern println(x);
func main() {
  while (has_more() == 1) {
    var a = next_a();
    var op = next_op();
    var b = next_b();
    var result = 0;
    if (op == "+") {
      result = a + b;
    } else {
      if (op == "-") {
        result = a - b;
      } else {
        if (op == "*") {
          result = a * b;
        } else {
          result = a / b;
        }
      }
    }
    println(result);
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
        vm->addFunction("has_more", kern::createExternFunction(&has_more));
        vm->addFunction("next_a", kern::createExternFunction(&next_a));
        vm->addFunction("next_op", kern::createExternFunction(&next_op));
        vm->addFunction("next_b", kern::createExternFunction(&next_b));
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
