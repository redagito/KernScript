#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <kern/Kern.h>

// Concrete type for the optional --profile dumps below.
#include "vm/VirtualMachine.h"

#include "../../common/bench.h"

namespace
{

constexpr int kFibN = 20;
constexpr int kExpectedFib = 6765;

// Depth of the generated call chain c199 -> ... -> c0. Under the default
// 1024 call depth limit with room to spare.
constexpr int kChainDepth = 200;
constexpr int kExpectedChain = 200;
// Chain traversals per timing repeat.
constexpr int kChainRuns = 500;

long long g_captured = 0;
long long g_captureCalls = 0;

void capture(int value)
{
    g_captured = value;
    ++g_captureCalls;
}

const std::string kFibSource = R"(extern capture(x);
func fib(n) {
  if (n <= 1) {
    return n;
  }
  var a = fib(n - 1);
  var b = fib(n - 2);
  return a + b;
}
func main() {
  capture(fib(20));
}
)";

// Builds funcs c0..c199 where c0 returns 1 and each next returns the
// previous plus 1, plus a main capturing c199: a 200-deep call chain with
// the known result 200.
std::string buildChainSource()
{
    std::ostringstream source;
    source << "extern capture(x);\n";
    source << "func c0() {\n  return 1;\n}\n";
    for (int i = 1; i < kChainDepth; ++i)
    {
        source << "func c" << i << "() {\n  return c" << (i - 1) << "() + 1;\n}\n";
    }
    source << "func main() {\n  capture(c" << (kChainDepth - 1) << "());\n}\n";
    return source.str();
}

} // namespace

int main(int argumentCount, char **arguments)
{
    std::cout << "machine: " << bench::machineNote() << '\n';
    const bool profile = argumentCount > 1 && std::string(arguments[1]) == "--profile";
    int exitCode = 0;

    // Part 1: recursive fib(20).
    {
        g_captured = 0;
        g_captureCalls = 0;
        std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm();
        vm->addFunction("capture", kern::createExternFunction(&capture));
        if (!bench::compileLoad(kFibSource, *vm))
        {
            std::cerr << "NestedCalls/fib: pipeline failed\n";
            return 1;
        }
        try
        {
            if (!vm->callFunction("main"))
            {
                std::cerr << "NestedCalls/fib: checksum FAIL, main not found\n";
                return 1;
            }
        }
        catch (const kern::RuntimeError &error)
        {
            std::cerr << "NestedCalls/fib: checksum FAIL, " << error.what() << '\n';
            return 1;
        }
        if (g_captured != kExpectedFib)
        {
            std::cerr << "NestedCalls/fib: checksum FAIL, captured " << g_captured << '\n';
            return 1;
        }

        // Optional opcode profile of the timed region; off by default.
        // Profiling adds probe overhead to the timing and is relative only.
        if (profile)
        {
            static_cast<VirtualMachine *>(vm.get())->resetProfile();
        }

        const double nsPerFib = bench::minNsPerOp(
            [&]
            {
                try
                {
                    vm->callFunction("main");
                }
                catch (const kern::RuntimeError &)
                {
                    // Counted as missing below: the call count falls short.
                }
            },
            1);

        const long long expectedCalls = 1 + 1 + bench::kRepeats;
        const bool checksumOk = (g_captured == kExpectedFib) && (g_captureCalls == expectedCalls);
        if (profile)
        {
            std::cout << "--- profile fib20 ---\n";
            static_cast<VirtualMachine *>(vm.get())->dumpProfile(std::cout);
        }
        exitCode |= bench::report("NestedCalls/fib20", nsPerFib, 1, checksumOk);
    }

    // Part 2: deep call chain.
    {
        g_captured = 0;
        g_captureCalls = 0;
        std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm();
        vm->addFunction("capture", kern::createExternFunction(&capture));
        if (!bench::compileLoad(buildChainSource(), *vm))
        {
            std::cerr << "NestedCalls/chain: pipeline failed\n";
            return 1;
        }
        try
        {
            if (!vm->callFunction("main"))
            {
                std::cerr << "NestedCalls/chain: checksum FAIL, main not found\n";
                return 1;
            }
        }
        catch (const kern::RuntimeError &error)
        {
            std::cerr << "NestedCalls/chain: checksum FAIL, " << error.what() << '\n';
            return 1;
        }
        if (g_captured != kExpectedChain)
        {
            std::cerr << "NestedCalls/chain: checksum FAIL, captured " << g_captured << '\n';
            return 1;
        }

        // Optional opcode profile of the timed region; off by default.
        if (profile)
        {
            static_cast<VirtualMachine *>(vm.get())->resetProfile();
        }

        const double nsPerChain = bench::minNsPerOp(
            [&]
            {
                for (int run = 0; run < kChainRuns; ++run)
                {
                    try
                    {
                        vm->callFunction("main");
                    }
                    catch (const kern::RuntimeError &)
                    {
                        // Counted as missing below: the call count falls short.
                    }
                }
            },
            kChainRuns);

        const long long expectedCalls = 1 + static_cast<long long>(kChainRuns) * (1 + bench::kRepeats);
        const bool checksumOk = (g_captured == kExpectedChain) && (g_captureCalls == expectedCalls);
        if (profile)
        {
            std::cout << "--- profile chain200 ---\n";
            static_cast<VirtualMachine *>(vm.get())->dumpProfile(std::cout);
        }
        exitCode |= bench::report("NestedCalls/chain200", nsPerChain, kChainRuns, checksumOk);
    }

    return exitCode;
}
