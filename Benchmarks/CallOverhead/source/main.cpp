#include <iostream>
#include <memory>
#include <string>

#include <kern/Kern.h>

#include "../../common/bench.h"

namespace
{

// Calls of one empty main per timing run. Shared with main for the checksum.
constexpr long long kCalls = 100000;

const std::string kSource = "func main() {}";

} // namespace

int main()
{
    std::cout << "machine: " << bench::machineNote() << '\n';

    std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm();
    if (!bench::compileLoad(kSource, *vm))
    {
        std::cerr << "CallOverhead: pipeline failed\n";
        return 1;
    }

    // CHECKSUM: the empty call must report success. Every timed call below
    // counts its success, so the final count proves the timed work is real.
    long long successes = 0;
    try
    {
        if (!vm->callFunction("main"))
        {
            std::cerr << "CallOverhead: checksum FAIL, main not found\n";
            return 1;
        }
        ++successes;
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "CallOverhead: checksum FAIL, " << error.what() << '\n';
        return 1;
    }

    const double nsPerCall = bench::minNsPerOp(
        [&]
        {
            for (long long i = 0; i < kCalls; ++i)
            {
                try
                {
                    if (vm->callFunction("main"))
                    {
                        ++successes;
                    }
                }
                catch (const kern::RuntimeError &)
                {
                    // Counted as missing below: successes fall short.
                }
            }
        },
        kCalls);

    // The pre-timing checksum call above counts one success already.
    const long long expected = kCalls * (1 + bench::kRepeats) + 1;
    const bool checksumOk = (successes == expected);
    return bench::report("CallOverhead", nsPerCall, kCalls, checksumOk);
}
