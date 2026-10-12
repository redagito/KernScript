#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <kern/Kern.h>

#include "../../common/bench.h"

namespace
{

// Generated functions f0..f59, each returning its index on its own lines,
// plus a main summing them all: 59 * 60 / 2 = 1770.
constexpr int kFunctionCount = 60;
constexpr int kExpectedSum = 1770;

long long g_captured = 0;

void put(int value) { g_captured = value; }

std::string buildSource()
{
    std::ostringstream source;
    source << "extern put(x);\n";
    for (int i = 0; i < kFunctionCount; ++i)
    {
        source << "func f" << i << "() {\n";
        source << "  return " << i << ";\n";
        source << "}\n";
        source << "\n";
    }
    source << "func main() {\n";
    source << "  var t = f0() + f1() + f2() + f3() + f4() + f5() + f6() + f7() + f8() + f9();\n";
    for (int base = 10; base < kFunctionCount; base += 10)
    {
        source << "  t = t + f" << base << "() + f" << (base + 1) << "() + f" << (base + 2) << "() + f" << (base + 3)
               << "() + f" << (base + 4) << "() + f" << (base + 5) << "() + f" << (base + 6) << "() + f" << (base + 7)
               << "() + f" << (base + 8) << "() + f" << (base + 9) << "();\n";
    }
    source << "  put(t);\n";
    source << "}\n";
    return source.str();
}

long long countLines(const std::string &text)
{
    long long lines = 0;
    for (char c : text)
    {
        if (c == '\n')
        {
            ++lines;
        }
    }
    return lines;
}

// CHECKSUM: the source compiles, assembles, loads, and main produces the
// known sum. Runs once before timing and once after.
bool verifyOutput(const std::string &source)
{
    std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm();
    vm->addFunction("put", kern::createExternFunction(&put));
    if (!bench::compileLoad(source, *vm))
    {
        std::cerr << "Pipeline: checksum FAIL, pipeline failed\n";
        return false;
    }
    try
    {
        if (!vm->callFunction("main"))
        {
            std::cerr << "Pipeline: checksum FAIL, main not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "Pipeline: checksum FAIL, " << error.what() << '\n';
        return false;
    }
    if (g_captured != kExpectedSum)
    {
        std::cerr << "Pipeline: checksum FAIL, captured " << g_captured << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    std::cout << "machine: " << bench::machineNote() << '\n';

    const std::string source = buildSource();
    const long long lineCount = countLines(source);
    std::cout << "Pipeline: source lines " << lineCount << ", bytes " << source.size() << '\n';
    if (lineCount < 200)
    {
        std::cerr << "Pipeline: source smaller than the 200-line target\n";
        return 1;
    }

    if (!verifyOutput(source))
    {
        return 1;
    }

    // Timed section: min of N compile+assemble+load runs (warmup first).
    // The main run stays outside the stopwatch; its output was verified
    // above and is verified again below, and every timed iteration asserts
    // its own load.
    double bestNs = -1.0;
    for (int i = 0; i < 1 + bench::kRepeats; ++i)
    {
        std::unique_ptr<kern::IVirtualMachine> timedVm = bench::makeVm();
        const auto start = std::chrono::steady_clock::now();
        const bool ok = bench::compileLoad(source, *timedVm);
        const auto end = std::chrono::steady_clock::now();
        if (!ok)
        {
            std::cerr << "Pipeline: checksum FAIL, timed load failed\n";
            return 1;
        }
        if (i == 0)
        {
            continue; // Warmup, not timed.
        }
        const double ns =
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        if (bestNs < 0.0 || ns < bestNs)
        {
            bestNs = ns;
        }
    }

    if (!verifyOutput(source))
    {
        return 1;
    }
    return bench::report("Pipeline", bestNs, 1, true);
}
