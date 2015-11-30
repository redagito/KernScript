#include <fstream>
#include <iostream>
#include <memory>
#include <iostream>

#include <kern/Kern.h>

/**
* \brief Assembles source file and stores bytecode in output file.
*/
bool assemble(kern::IAssembler &assembler, const std::string &asmSource,
              const std::string &byteCode)
{
  // Open streams
  std::ifstream ifs(asmSource);
  if (!ifs.is_open())
  {
    std::cout << "Failed to open the input file " << asmSource << std::endl;
    return false;
  }

  std::ofstream ofs(byteCode, std::ios::binary);
  if (!ofs.is_open())
  {
    std::cout << "Failed to open the output file " << byteCode << std::endl;
    return false;
  }

  // Assemble input into output stream
  if (!assembler.assemble(ifs, ofs))
  {
    std::cout << "Failed to assemble the input file " << asmSource << std::endl;
    return false;
  }
  return true;
}

/**
* \brief Executes main function of bytecode script.
*/
bool execute(kern::IVirtualMachine &vm, const std::string &byteCode)
{
  std::ifstream ifs(byteCode, std::ios::binary);
  if (!ifs.is_open())
  {
    std::cout << "Failed to open the input file " << byteCode << std::endl;
    return false;
  }
  if (!vm.load(ifs))
  {
    std::cout << "Failed to load the input file " << byteCode << std::endl;
    return false;
  }
  if (!vm.callFunction("main"))
  {
    std::cout << "Failed to execute the file " << byteCode << std::endl;
    return false;
  }
  return true;
}

/**
* \brief Assembles and executes assembly script.
*/
bool testAsm(kern::IAssembler &assembler, kern::IVirtualMachine &vm,
             const std::string &asmCode, const std::string &byteCode)
{
  if (!assemble(assembler, asmCode, byteCode))
  {
    std::cout << "Assembly failed" << std::endl;
    return false;
  }

  if (!execute(vm, byteCode))
  {
    std::cout << "Execute failed" << std::endl;
    return false;
  }
  return true;
}

/**
* \brief Starts testcase by name.
*/
bool testAsmUtil(kern::IAssembler &assembler, kern::IVirtualMachine &vm,
                 const std::string &testName)
{
  vm.clearScripts();
  std::cout << std::endl
            << "--- Starting test case: " << testName << std::endl;

  bool ok = true;
  if (!testAsm(assembler, vm, "test_data/testasm/" + testName + ".kasm",
               "test_data/testasm_out/" + testName + ".kbyte"))
  {
    ok = false;
  }
  std::cout << "Test case: testasm '" << testName << "' ";
  if (ok)
  {
    std::cout << "SUCCEEDED" << std::endl;
  }
  else
  {
    std::cout << "FAILED" << std::endl;
  }
  return true;
}

/**
* \brief Utility print function to be callable from script.
*/
void print(std::string text) { std::cout << text; }

void println(std::string text) { std::cout << text << std::endl; }

void log(std::string severity, std::string message)
{
  std::cout << "[" << severity << "]: " << message << std::endl;
}

int get_value() { return 35; }

int fast_fib(int v)
{
  if (v < 2)
    return v;
  return fast_fib(v - 1) + fast_fib(v - 2);
}

int main(int argc, char **argv)
{
  std::unique_ptr<kern::ICompiler> comp(kern::createCompiler());
  std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
  std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());

  // Utility print function
  vm->addFunction("print", kern::createExternFunction(print));
  vm->addFunction("println", kern::createExternFunction(println));
  vm->addFunction("log", kern::createExternFunction(log));
  vm->addFunction("get_value", kern::createExternFunction(get_value));
  vm->addFunction("fast_fib", kern::createExternFunction(fast_fib));

  testAsmUtil(*assembler, *vm, "test1");
  testAsmUtil(*assembler, *vm, "test2");
  testAsmUtil(*assembler, *vm, "test3");
  testAsmUtil(*assembler, *vm, "test4");
  testAsmUtil(*assembler, *vm, "test5");
  testAsmUtil(*assembler, *vm, "test6");
  testAsmUtil(*assembler, *vm, "test7");
  testAsmUtil(*assembler, *vm, "fibonacci");

  return 0;
}
