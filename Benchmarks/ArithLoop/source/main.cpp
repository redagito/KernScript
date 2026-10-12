#include <iostream>
#include <memory>
#include <string>

#include <kern/Kern.h>

#include "../../common/bench.h"

namespace
{

// The inner loop runs 1000 times per outer iteration over 1000 outer
// iterations: exactly 1000000 accumulation iterations per main run. The nest
// keeps every partial sum in the 32-bit range: each inner pass adds the
// triangular sum 1000 * 1001 / 2 = 500500, so the total is
// 1000 * 500500 = 500500000. A single flat 1M loop summing i would need
// N*(N+1)/2 = 500000500000, far past the 32-bit range, and the engine
// correctly fails out-of-range results instead of wrapping.
constexpr int kInnerIterations = 1000000;
constexpr int kExpectedTotal = 500500000;

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
  var o = 0;
  while (o < 1000) {
    var i = 1;
    while (i <= 1000) {
      total = total + i;
      i = i + 1;
    }
    o = o + 1;
  }
  capture(total);
}
)";

} // namespace

int main()
{
    std::cout << "machine: " << bench::machineNote() << '\n';

    // About a dozen instructions per inner iteration over 1M iterations:
    // past the default 10M execution step budget, so raise it explicitly.
    kern::VMConfig config;
    config.maxExecutionSteps = 100000000;
    std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm(config);
    vm->addFunction("capture", kern::createExternFunction(&capture));
    if (!bench::compileLoad(kSource, *vm))
    {
        std::cerr << "ArithLoop: pipeline failed\n";
        return 1;
    }

    try
    {
        if (!vm->callFunction("main"))
        {
            std::cerr << "ArithLoop: checksum FAIL, main not found\n";
            return 1;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "ArithLoop: checksum FAIL, " << error.what() << '\n';
        return 1;
    }
    if (g_captured != kExpectedTotal)
    {
        std::cerr << "ArithLoop: checksum FAIL, captured " << g_captured << '\n';
        return 1;
    }

    const double nsPerIter = bench::minNsPerOp(
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
        kInnerIterations);

    const long long expectedCalls = 1 + 1 + bench::kRepeats;
    const bool checksumOk = (g_captured == kExpectedTotal) && (g_captureCalls == expectedCalls);
    return bench::report("ArithLoop", nsPerIter, kInnerIterations, checksumOk);
}
