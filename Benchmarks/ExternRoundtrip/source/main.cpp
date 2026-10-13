#include <iostream>
#include <memory>
#include <string>

#include <kern/Kern.h>

// Concrete type for the optional --profile dump below.
#include "vm/VirtualMachine.h"

#include "../../common/bench.h"

namespace
{

// Script loop iterations per main run; each iteration makes one valueless
// tick() call and one valued succ(i) call.
constexpr int kIterations = 50000;
// main runs per timing repeat.
constexpr int kRuns = 2;

// acc sums succ(i) = i + 1 over 0..kIterations-1:
// 50000 * 50001 / 2 = 1250025000, inside the 32-bit range.
constexpr int kExpectedAcc = 1250025000;

long long g_tickCalls = 0;
long long g_succCalls = 0;
long long g_captured = 0;
long long g_captureCalls = 0;

void tick() { ++g_tickCalls; }

int succ(int value)
{
    ++g_succCalls;
    return value + 1;
}

void capture(int value)
{
    g_captured = value;
    ++g_captureCalls;
}

const std::string kSource = R"(extern tick();
extern succ(x);
extern capture(x);
func main() {
  var acc = 0;
  var i = 0;
  while (i < 50000) {
    tick();
    var v = succ(i);
    acc = acc + v;
    i = i + 1;
  }
  capture(acc);
}
)";

} // namespace

int main(int argumentCount, char **arguments)
{
    std::cout << "machine: " << bench::machineNote() << '\n';
    const bool profile = argumentCount > 1 && std::string(arguments[1]) == "--profile";

    std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm();
    vm->addFunction("tick", kern::createExternFunction(&tick));
    vm->addFunction("succ", kern::createExternFunction(&succ));
    vm->addFunction("capture", kern::createExternFunction(&capture));
    if (!bench::compileLoad(kSource, *vm))
    {
        std::cerr << "ExternRoundtrip: pipeline failed\n";
        return 1;
    }

    try
    {
        if (!vm->callFunction("main"))
        {
            std::cerr << "ExternRoundtrip: checksum FAIL, main not found\n";
            return 1;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "ExternRoundtrip: checksum FAIL, " << error.what() << '\n';
        return 1;
    }
    if (g_captured != kExpectedAcc || g_tickCalls != kIterations || g_succCalls != kIterations)
    {
        std::cerr << "ExternRoundtrip: checksum FAIL\n";
        return 1;
    }

    // Two extern calls per iteration: report ns per extern call.
    // Optional opcode profile of the timed region; off by default.
    // Profiling adds probe overhead to the timing and is relative only.
    if (profile)
    {
        static_cast<VirtualMachine *>(vm.get())->resetProfile();
    }
    const long long opsPerRun = static_cast<long long>(kIterations) * 2 * kRuns;
    const double nsPerCall = bench::minNsPerOp(
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
                    // Counted as missing below: the call counts fall short.
                }
            }
        },
        opsPerRun);

    const long long mains = 1 + static_cast<long long>(kRuns) * (1 + bench::kRepeats);
    const bool checksumOk = (g_captured == kExpectedAcc) && (g_captureCalls == mains) &&
                            (g_tickCalls == mains * kIterations) && (g_succCalls == mains * kIterations);
    if (profile)
    {
        std::cout << "--- profile externroundtrip ---\n";
        static_cast<VirtualMachine *>(vm.get())->dumpProfile(std::cout);
    }
    return bench::report("ExternRoundtrip", nsPerCall, opsPerRun, checksumOk);
}
