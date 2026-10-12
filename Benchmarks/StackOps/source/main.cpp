#include <iostream>
#include <memory>
#include <string>

#include <kern/Kern.h>

#include "../../common/bench.h"

namespace
{

// Loop iterations per main run. Each iteration evaluates a compound
// expression (several pushes and pops) and folds it into the total, so the
// loop stresses expression stack traffic rather than bare arithmetic.
constexpr int kIterations = 50000;
// main runs per timing repeat; keeps one repeat well above timer resolution.
constexpr int kRuns = 3;

// t unfolds to exactly i (5i - 3i + 2i - 3i), so total is the triangular
// sum over 0..kIterations-1: 50000 * 49999 / 2 = 1249975000, which fits the
// 32-bit range with room to spare.
constexpr int kExpectedTotal = 1249975000;

long long g_captured = 0;
long long g_captureCalls = 0;

void capture(int value)
{
    g_captured = value;
    ++g_captureCalls;
}

const std::string kSource = R"(extern capture(x);
func main() {
  var total = 0;
  var i = 0;
  while (i < 50000) {
    var t = i * 5 - i * 3 + i * 2 - i * 3;
    total = total + t;
    i = i + 1;
  }
  capture(total);
}
)";

} // namespace

int main()
{
    std::cout << "machine: " << bench::machineNote() << '\n';

    std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm();
    vm->addFunction("capture", kern::createExternFunction(&capture));
    if (!bench::compileLoad(kSource, *vm))
    {
        std::cerr << "StackOps: pipeline failed\n";
        return 1;
    }

    try
    {
        if (!vm->callFunction("main"))
        {
            std::cerr << "StackOps: checksum FAIL, main not found\n";
            return 1;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "StackOps: checksum FAIL, " << error.what() << '\n';
        return 1;
    }
    if (g_captured != kExpectedTotal)
    {
        std::cerr << "StackOps: checksum FAIL, captured " << g_captured << '\n';
        return 1;
    }

    const long long opsPerRun = static_cast<long long>(kIterations) * kRuns;
    const double nsPerPush = bench::minNsPerOp(
        [&]
        {
            for (int run = 0; run < kRuns; ++run)
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
        opsPerRun);

    const long long expectedCalls = 1 + static_cast<long long>(kRuns) * (1 + bench::kRepeats);
    const bool checksumOk = (g_captured == kExpectedTotal) && (g_captureCalls == expectedCalls);
    return bench::report("StackOps", nsPerPush, opsPerRun, checksumOk);
}
