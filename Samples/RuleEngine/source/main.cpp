#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

namespace
{

struct Order
{
    int total;
    std::string kind;
};

std::vector<Order> g_orders = {{120, "std"}, {200, "vip"}, {80, "staff"}, {50, "vip"}};
std::size_t g_current = 0;
int g_captured = 0;

int order_total() { return g_orders[g_current].total; }

std::string order_kind() { return g_orders[g_current].kind; }

void emit(int value) { g_captured = value; }

} // namespace

int main()
{
    const std::string source = R"(extern order_total();
extern order_kind();
extern emit(x);
func price() {
  var total = order_total();
  var kind = order_kind();
  var result = total;
  if (kind == "vip") {
    if (total >= 100) {
      result = total * 80 / 100;
    } else {
      result = total * 90 / 100;
    }
  } else {
    if (kind == "staff") {
      result = total * 70 / 100;
    } else {
      if (total >= 100) {
        result = total * 95 / 100;
      } else {
        result = total;
      }
    }
  }
  emit(result);
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
        vm->addFunction("order_total", kern::createExternFunction(&order_total));
        vm->addFunction("order_kind", kern::createExternFunction(&order_kind));
        vm->addFunction("emit", kern::createExternFunction(&emit));
        if (!vm->load(byteCode))
        {
            std::cerr << "load failed\n";
            return 1;
        }
        for (std::size_t i = 0; i < g_orders.size(); ++i)
        {
            g_current = i;
            if (!vm->callFunction("price"))
            {
                std::cerr << "function price not found\n";
                return 1;
            }
            std::cout << "order " << i << ": total " << g_orders[i].total << " kind " << g_orders[i].kind << " -> "
                      << g_captured << '\n';
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
