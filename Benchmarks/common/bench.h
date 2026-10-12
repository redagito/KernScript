#pragma once

// Shared header-only harness for the Benchmarks/ umbrella. Every benchmark
// asserts a CHECKSUM (a known-correct result) before printing timing:
// fast+wrong is a failure. Timing is min-of-N wall time via chrono,
// reported as ns/op plus ops/s plus the checksum verdict. No third-party
// dependencies: only the standard library and the public kern API.

#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <kern/Kern.h>

namespace bench
{

// Timing repeats; the minimum wins, which suppresses one-sided OS noise.
inline constexpr int kRepeats = 7;

// Runs op once untimed (warmup), then repeats times, returning the minimum
// wall time per op in nanoseconds. Returns a negative value when the counts
// are not positive so callers fail loudly instead of dividing by zero.
template <typename Op> double minNsPerOp(Op &&op, long long opsPerRun, int repeats = kRepeats)
{
    if (opsPerRun <= 0 || repeats <= 0)
    {
        return -1.0;
    }
    op();
    double bestNsPerOp = -1.0;
    for (int i = 0; i < repeats; ++i)
    {
        const auto start = std::chrono::steady_clock::now();
        op();
        const auto end = std::chrono::steady_clock::now();
        const double ns =
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        const double nsPerOp = ns / static_cast<double>(opsPerRun);
        if (bestNsPerOp < 0.0 || nsPerOp < bestNsPerOp)
        {
            bestNsPerOp = nsPerOp;
        }
    }
    return bestNsPerOp;
}

// Builds a virtual machine that discards diagnostics.
inline std::unique_ptr<kern::IVirtualMachine> makeVm()
{
    return std::unique_ptr<kern::IVirtualMachine>(kern::createVirtualMachine(nullptr));
}

// Builds a virtual machine with explicit limits that discards diagnostics.
// Used by benches whose step count exceeds the default execution budget.
inline std::unique_ptr<kern::IVirtualMachine> makeVm(kern::VMConfig config)
{
    return std::unique_ptr<kern::IVirtualMachine>(kern::createVirtualMachine(nullptr, config));
}

// Compiles source, assembles it and loads it into vm. All diagnostics are
// discarded. Returns false when any stage fails.
inline bool compileLoad(const std::string &source, kern::IVirtualMachine &vm)
{
    std::stringstream asmCode;
    {
        std::istringstream input(source);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
        if (!compiler->compile(input, asmCode))
        {
            return false;
        }
    }
    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler(nullptr));
        if (!assembler->assemble(input, byteCode))
        {
            return false;
        }
    }
    return vm.load(byteCode);
}

// Prints one result line and returns the process exit code for it: 0 when
// the checksum holds and the timing is sane, 1 otherwise. A non-positive
// ns/op means the timer never advanced: a harness bug, never a result.
inline int report(const std::string &name, double nsPerOp, long long opsPerRun, bool checksumOk)
{
    const bool sane = nsPerOp > 0.0 && opsPerRun > 0;
    const bool ok = checksumOk && sane;
    std::cout << name << ": ";
    if (sane)
    {
        std::cout << nsPerOp << " ns/op, " << 1e9 / nsPerOp << " ops/s";
    }
    else
    {
        std::cout << "no timing";
    }
    std::cout << " (ops " << opsPerRun << ", checksum " << (checksumOk ? "OK" : "FAIL");
    if (!sane)
    {
        std::cout << ", TIMER SUSPECT";
    }
    std::cout << ")\n";
    return ok ? 0 : 1;
}

// One-line machine note for benchmark log provenance. The full baseline
// record (CPU model, RAM, OS, date, commit) lives in Benchmarks/README.md.
inline std::string machineNote()
{
    std::string os = "unknown OS";
#if defined(_WIN32)
    os = "Windows";
#elif defined(__linux__)
    os = "Linux";
#elif defined(__APPLE__)
    os = "macOS";
#endif
    std::string arch = "unknown arch";
#if defined(_M_X64) || defined(__x86_64__)
    arch = "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    arch = "ARM64";
#endif
    return os + " " + arch + ", " + std::to_string(std::thread::hardware_concurrency()) + " hardware threads";
}

} // namespace bench
