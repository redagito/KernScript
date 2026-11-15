#include <kern/Kern.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef KERN_BENCH_MEMORY
#include "Allocations.h"
#endif

namespace
{
using Clock = std::chrono::steady_clock;
constexpr int kSamples = 9;
constexpr int kStringIterations = 1000;

struct Times
{
    double minimum = 0;
    double median = 0;
    double maximum = 0;
};

struct Workload
{
    int functionCount;
    std::string source;
    std::string assembly;
    std::string bytecode;
    int expected;
};

long long g_captured = 0;
long long g_captureCalls = 0;
void capture(int value)
{
    g_captured = value;
    ++g_captureCalls;
}

std::string buildSource(int functionCount)
{
    std::ostringstream source;
    source << "extern capture(x) -> void;\n";
    for (int i = 0; i < functionCount; ++i)
    {
        source << "func f" << i << "() -> value { return " << i << "; }\n";
    }
    source << "func main() -> void { var total = 0;\n";
    for (int i = 0; i < functionCount; ++i)
        source << "total = total + f" << i << "();\n";
    source << "capture(total); }\n";
    return source.str();
}

bool compile(const std::string &source, std::string &assembly)
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
    std::istringstream input(source);
    std::ostringstream output;
    const bool ok = compiler->compile(input, output);
    assembly = output.str();
    return ok;
}

bool assemble(const std::string &assembly, std::string &bytecode)
{
    std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler(nullptr));
    std::istringstream input(assembly);
    std::ostringstream output;
    const bool ok = assembler->assemble(input, output);
    bytecode = output.str();
    return ok;
}

bool compileBytecode(const std::string &source, std::string &bytecode)
{
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
    std::istringstream input(source);
    std::ostringstream output;
    const bool ok = compiler->compileBytecode(input, output);
    bytecode = output.str();
    return ok;
}

std::unique_ptr<kern::IVirtualMachine> makeVm()
{
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(nullptr));
    vm->addFunction("capture", &capture);
    return vm;
}

bool loadBytes(kern::IVirtualMachine &vm, const std::string &bytecode)
{
    std::istringstream input(bytecode);
    return vm.load(input);
}

bool verifyProgram(const std::string &source, int expected)
{
    std::string bytecode;
    if (!compileBytecode(source, bytecode))
        return false;
    auto vm = makeVm();
    if (!loadBytes(*vm, bytecode))
        return false;
    g_captured = 0;
    g_captureCalls = 0;
    try
    {
        return vm->callFunction("main") && g_captured == expected && g_captureCalls == 1;
    }
    catch (const kern::RuntimeError &)
    {
        return false;
    }
}

template <typename Prepare, typename Operation, typename Verify>
Times measure(Prepare prepare, Operation operation, Verify verify, bool &ok)
{
    auto sample = [&]()
    {
        prepare();
        const auto start = Clock::now();
        const bool result = operation();
        const auto end = Clock::now();
        const double elapsed = std::chrono::duration<double, std::nano>(end - start).count();
        ok = ok && result && elapsed > 0 && verify();
        return elapsed;
    };
    sample(); // warmup
    std::vector<double> values;
    values.reserve(kSamples);
    for (int i = 0; i < kSamples; ++i)
        values.push_back(sample());
    std::sort(values.begin(), values.end());
    return {values.front(), values[values.size() / 2], values.back()};
}

void printTime(const char *stage, int dimension, const Workload &data, const Times &times, bool ok)
{
    std::cout << "timing,generated_program," << stage << ',' << dimension << ',' << times.minimum << ',' << times.median
              << ',' << times.maximum << ",1," << data.source.size() << ',' << data.assembly.size() << ','
              << data.bytecode.size() << ',' << (ok ? "OK" : "FAIL") << '\n';
}

Workload prepareWorkload(int count)
{
    Workload data{count, buildSource(count), {}, {}, count * (count - 1) / 2};
    if (!compile(data.source, data.assembly) || !assemble(data.assembly, data.bytecode))
        throw std::runtime_error("workload compilation failed");
    if (!verifyProgram(data.source, data.expected))
        throw std::runtime_error("generated program checksum failed");
    return data;
}

bool speedPipeline(const Workload &data)
{
    bool allOk = true;
    bool ok = true;
    std::string output;
    std::istringstream sourceInput(data.source);
    std::ostringstream bytecodeOutput;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
    auto bytecodePrepare = [&]
    {
        sourceInput.clear();
        sourceInput.seekg(0);
        bytecodeOutput.str("");
        bytecodeOutput.clear();
        bytecodeOutput.seekp(0);
    };
    const auto bytecodeTimes = measure(
        bytecodePrepare, [&] { return compiler->compileBytecode(sourceInput, bytecodeOutput); },
        [&] { return bytecodeOutput.str() == data.bytecode; }, ok);
    output = bytecodeOutput.str();
    allOk = allOk && ok && output == data.bytecode;
    printTime("compile_bytecode", data.functionCount, data, bytecodeTimes, allOk);

    ok = true;
    std::istringstream sourceForAsm(data.source);
    std::ostringstream assemblyOutput;
    auto compilePrepare = [&]
    {
        sourceForAsm.clear();
        sourceForAsm.seekg(0);
        assemblyOutput.str("");
        assemblyOutput.clear();
        assemblyOutput.seekp(0);
    };
    const auto compileTimes = measure(
        compilePrepare, [&] { return compiler->compile(sourceForAsm, assemblyOutput); },
        [&] { return assemblyOutput.str() == data.assembly; }, ok);
    output = assemblyOutput.str();
    allOk = allOk && ok && output == data.assembly;
    printTime("compile_assembly", data.functionCount, data, compileTimes, allOk);

    ok = true;
    std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler(nullptr));
    std::istringstream assemblyInput(data.assembly);
    std::ostringstream assembledOutput;
    auto assemblePrepare = [&]
    {
        assemblyInput.clear();
        assemblyInput.seekg(0);
        assembledOutput.str("");
        assembledOutput.clear();
        assembledOutput.seekp(0);
    };
    const auto assembleTimes = measure(
        assemblePrepare, [&] { return assembler->assemble(assemblyInput, assembledOutput); },
        [&] { return assembledOutput.str() == data.bytecode; }, ok);
    output = assembledOutput.str();
    allOk = allOk && ok && output == data.bytecode;
    printTime("assemble", data.functionCount, data, assembleTimes, allOk);

    ok = true;
    auto loadVm = makeVm();
    std::istringstream loadInput(data.bytecode);
    auto loadPrepare = [&]
    {
        loadInput.clear();
        loadInput.seekg(0);
    };
    auto verifyVm = [&](kern::IVirtualMachine &candidate)
    {
        g_captured = 0;
        g_captureCalls = 0;
        try
        {
            return candidate.callFunction("main") && g_captured == data.expected && g_captureCalls == 1;
        }
        catch (const kern::RuntimeError &)
        {
            return false;
        }
    };
    const auto loadTimes =
        measure(loadPrepare, [&] { return loadVm->load(loadInput); }, [&] { return verifyVm(*loadVm); }, ok);
    allOk = allOk && ok;
    printTime("load", data.functionCount, data, loadTimes, allOk);

    ok = true;
    auto directVm = makeVm();
    std::istringstream directSource(data.source);
    std::stringstream directOutput;
    auto directPrepare = [&]
    {
        directSource.clear();
        directSource.seekg(0);
        directOutput.str("");
        directOutput.clear();
        directOutput.seekg(0);
        directOutput.seekp(0);
    };
    const auto directTimes = measure(
        directPrepare,
        [&]
        {
            if (!compiler->compileBytecode(directSource, directOutput))
                return false;
            directOutput.clear();
            directOutput.seekg(0);
            return directVm->load(directOutput);
        },
        [&] { return verifyVm(*directVm); }, ok);
    allOk = allOk && ok;
    printTime("compile_load", data.functionCount, data, directTimes, allOk);

    ok = true;
    auto textVm = makeVm();
    std::istringstream textSource(data.source);
    std::stringstream textAssembly;
    std::stringstream textBytecode;
    auto textPrepare = [&]
    {
        textSource.clear();
        textSource.seekg(0);
        textAssembly.str("");
        textAssembly.clear();
        textAssembly.seekg(0);
        textAssembly.seekp(0);
        textBytecode.str("");
        textBytecode.clear();
        textBytecode.seekg(0);
        textBytecode.seekp(0);
    };
    const auto textTimes = measure(
        textPrepare,
        [&]
        {
            if (!compiler->compile(textSource, textAssembly))
                return false;
            textAssembly.clear();
            textAssembly.seekg(0);
            if (!assembler->assemble(textAssembly, textBytecode))
                return false;
            textBytecode.clear();
            textBytecode.seekg(0);
            return textVm->load(textBytecode);
        },
        [&] { return verifyVm(*textVm); }, ok);
    allOk = allOk && ok;
    printTime("text_compile_load", data.functionCount, data, textTimes, allOk);
    return allOk;
}

bool lookupBenchmark()
{
    constexpr int calls = 10000;
    for (const int count : {10, 100, 1000})
    {
        std::ostringstream source;
        for (int i = 0; i < count; ++i)
            source << "func e" << i << "() -> void {}\n";
        std::string bytecode;
        if (!compileBytecode(source.str(), bytecode))
            return false;
        auto vm = makeVm();
        if (!loadBytes(*vm, bytecode))
            return false;
        const int middle = (count - 1) / 2;
        for (const int index : {0, middle, count - 1})
        {
            const std::string name = "e" + std::to_string(index);
            std::array<long long, kSamples + 1> successfulCalls{};
            int sampleIndex = 0;
            bool valid = true;
            auto times =
                measure([] {},
                        [&]
                        {
                            long long successes = 0;
                            try
                            {
                                for (int i = 0; i < calls; ++i)
                                    successes += vm->callFunction(name) ? 1 : 0;
                            }
                            catch (const kern::RuntimeError &)
                            {
                                return false;
                            }
                            successfulCalls[static_cast<std::size_t>(sampleIndex)] = successes;
                            return true;
                        },
                        [&] { return successfulCalls[static_cast<std::size_t>(sampleIndex++)] == calls; }, valid);
            std::cout << "timing,entry_lookup,entry_index_" << index << ',' << count << ',' << times.minimum / calls
                      << ',' << times.median / calls << ',' << times.maximum / calls << ',' << calls << ",0,0,"
                      << bytecode.size() << ',' << (valid ? "OK" : "FAIL") << '\n';
            if (!valid)
                return false;
        }
    }
    return true;
}

std::string g_hostString;
std::size_t g_stringLength = 0;
std::uint64_t g_stringChecksum = 0;
int g_stringGets = 0;
std::string getString()
{
    ++g_stringGets;
    return g_hostString;
}
void sink(const std::string &value)
{
    g_stringLength += value.size();
    for (unsigned char c : value)
        g_stringChecksum += c;
}

bool stringBenchmark(std::size_t length)
{
    g_hostString.assign(length, 'x');
    const std::string source = R"(extern getString() -> value;
extern sink(x) -> void;
func main() -> void {
  var i = 0;
  while (i < 1000) {
    var value = getString();
    sink(value);
    i = i + 1;
  }
})";
    std::string bytecode;
    if (!compileBytecode(source, bytecode))
        return false;
    auto vm = std::unique_ptr<kern::IVirtualMachine>(kern::createVirtualMachine(nullptr));
    vm->addFunction("getString", &getString);
    vm->addFunction("sink", &sink);
    if (!loadBytes(*vm, bytecode))
        return false;
    auto reset = []
    {
        g_stringGets = 0;
        g_stringLength = 0;
        g_stringChecksum = 0;
    };
    const auto expectedChecksum = static_cast<std::uint64_t>(length) * kStringIterations * 'x';
    auto verify = [&]
    {
        return g_stringGets == kStringIterations && g_stringLength == length * kStringIterations &&
               g_stringChecksum == expectedChecksum;
    };
    reset();
    try
    {
        if (!vm->callFunction("main") || !verify())
            return false;
    }
    catch (const kern::RuntimeError &)
    {
        return false;
    }

    bool ok = true;
    const auto times = measure(
        reset,
        [&]
        {
            try
            {
                return vm->callFunction("main");
            }
            catch (const kern::RuntimeError &)
            {
                return false;
            }
        },
        verify, ok);
    std::cout << "timing,string_roundtrip,host_string_" << length << "_bytes," << kStringIterations << ','
              << times.minimum / kStringIterations << ',' << times.median / kStringIterations << ','
              << times.maximum / kStringIterations << ',' << kStringIterations << ',' << source.size() << ",0,"
              << bytecode.size() << ',' << (ok ? "OK" : "FAIL") << '\n';
    return ok;
}

#ifdef KERN_BENCH_MEMORY
struct alignas(64) AlignedProbe
{
    unsigned char bytes[64];
};

void allocatorSelfCheck()
{
    const auto before = allocations::snapshot();
    auto *number = static_cast<int *>(::operator new(sizeof(int)));
    *number = 41;
    auto *aligned =
        static_cast<AlignedProbe *>(::operator new(sizeof(AlignedProbe), std::align_val_t{alignof(AlignedProbe)}));
    const auto during = allocations::snapshot();
    const bool valid = *number == 41 && reinterpret_cast<std::uintptr_t>(aligned) % alignof(AlignedProbe) == 0 &&
                       during.liveAllocations == before.liveAllocations + 2 &&
                       during.liveBytes == before.liveBytes + sizeof(int) + sizeof(AlignedProbe) &&
                       during.totalAllocations == before.totalAllocations + 2 &&
                       during.totalRequestedBytes == before.totalRequestedBytes + sizeof(int) + sizeof(AlignedProbe);
    ::operator delete(number);
    ::operator delete(aligned, std::align_val_t{alignof(AlignedProbe)});
    const auto after = allocations::snapshot();
    if (!valid || after.liveBytes != before.liveBytes || after.liveAllocations != before.liveAllocations)
        throw std::runtime_error("allocation replacement self-check failed");
}

void printMemory(const char *name, const allocations::Snapshot &start, const allocations::Snapshot &end)
{
    const auto net = static_cast<long long>(end.liveBytes) - static_cast<long long>(start.liveBytes);
    const auto netAllocations =
        static_cast<long long>(end.liveAllocations) - static_cast<long long>(start.liveAllocations);
    const auto peak = end.highWaterBytes > start.liveBytes ? end.highWaterBytes - start.liveBytes : 0;
    std::cout << "memory," << name << ',' << net << ',' << netAllocations << ',' << peak << ','
              << (end.totalAllocations - start.totalAllocations) << ','
              << (end.totalRequestedBytes - start.totalRequestedBytes) << '\n';
}

template <typename Operation> void allocationPhase(const char *name, Operation operation)
{
    const auto start = allocations::snapshot();
    allocations::resetPeak();
    operation();
    const auto end = allocations::snapshot();
    printMemory(name, start, end);
}

bool memoryRun()
{
    allocatorSelfCheck();
    std::cout << "record_type,phase,net_live_bytes,net_live_allocations,peak_above_start_bytes,allocation_calls,total_"
                 "requested_bytes\n";
    const Workload data = prepareWorkload(600);
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
    std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler(nullptr));
    std::istringstream src(data.source);
    std::ostringstream asmOut;
    allocationPhase("compile_to_assembly",
                    [&]
                    {
                        src.clear();
                        src.seekg(0);
                        asmOut.str("");
                        asmOut.clear();
                        if (!compiler->compile(src, asmOut))
                            throw std::runtime_error("compile failed");
                    });
    if (asmOut.str() != data.assembly)
        throw std::runtime_error("assembly checksum failed");
    std::istringstream asmIn(data.assembly);
    std::ostringstream codeOut;
    allocationPhase("assemble",
                    [&]
                    {
                        asmIn.clear();
                        asmIn.seekg(0);
                        codeOut.str("");
                        codeOut.clear();
                        if (!assembler->assemble(asmIn, codeOut))
                            throw std::runtime_error("assemble failed");
                    });
    if (codeOut.str() != data.bytecode)
        throw std::runtime_error("assembled bytecode checksum failed");
    std::ostringstream byteOut;
    std::istringstream byteSrc(data.source);
    allocationPhase("compile_bytecode",
                    [&]
                    {
                        byteSrc.clear();
                        byteSrc.seekg(0);
                        byteOut.str("");
                        byteOut.clear();
                        if (!compiler->compileBytecode(byteSrc, byteOut))
                            throw std::runtime_error("bytecode compile failed");
                    });
    if (byteOut.str() != data.bytecode)
        throw std::runtime_error("direct bytecode checksum failed");
    const std::string bytes = data.bytecode;
    std::unique_ptr<kern::IVirtualMachine> vm;
    allocationPhase("vm_construction", [&] { vm.reset(kern::createVirtualMachine(nullptr)); });
    std::istringstream loadInput(bytes);
    allocationPhase("load",
                    [&]
                    {
                        loadInput.clear();
                        loadInput.seekg(0);
                        if (!vm->load(loadInput))
                            throw std::runtime_error("load failed");
                    });
    vm->addFunction("capture", &capture);
    allocationPhase("first_execution",
                    [&]
                    {
                        g_captureCalls = 0;
                        if (!vm->callFunction("main") || g_captured != data.expected || g_captureCalls != 1)
                            throw std::runtime_error("execution checksum failed");
                    });
    allocationPhase("warm_100_executions",
                    [&]
                    {
                        g_captureCalls = 0;
                        for (int i = 0; i < 100; ++i)
                            if (!vm->callFunction("main") || g_captured != data.expected)
                                throw std::runtime_error("execution checksum failed");
                        if (g_captured != data.expected || g_captureCalls != 100)
                            throw std::runtime_error("execution checksum failed");
                    });
    allocationPhase("clear_scripts", [&] { vm->clearScripts(); });
    allocationPhase("vm_destruction", [&] { vm.reset(); });

    auto strings = [&](std::size_t length)
    {
        g_hostString.assign(length, 'x');
        const std::string source = R"(extern getString() -> value; extern sink(x) -> void;
func main() -> void { var i = 0; while (i < 1000) { var value = getString(); sink(value); i = i + 1; } })";
        std::string code;
        if (!compileBytecode(source, code))
            throw std::runtime_error("string compile failed");
        auto stringVm = std::unique_ptr<kern::IVirtualMachine>(kern::createVirtualMachine(nullptr));
        stringVm->addFunction("getString", &getString);
        stringVm->addFunction("sink", &sink);
        if (!loadBytes(*stringVm, code))
            throw std::runtime_error("string load failed");
        auto run = [&]
        {
            g_stringGets = 0;
            g_stringLength = 0;
            g_stringChecksum = 0;
            if (!stringVm->callFunction("main") || g_stringGets != kStringIterations ||
                g_stringLength != length * kStringIterations ||
                g_stringChecksum != static_cast<std::uint64_t>(length) * kStringIterations * 'x')
                throw std::runtime_error("string checksum failed");
        };
        allocationPhase(length == 16 ? "strings_16_first" : "strings_1024_first", run);
        for (int i = 0; i < 5; ++i)
            run();
        allocationPhase(length == 16 ? "strings_16_warm" : "strings_1024_warm", run);
    };
    strings(16);
    strings(1024);

    const std::string tinySource = "extern capture(x) -> void; func main() -> void { capture(17); }";
    std::string tinyBytecode;
    if (!compileBytecode(tinySource, tinyBytecode))
        throw std::runtime_error("tiny compile failed");
    std::vector<std::unique_ptr<kern::IVirtualMachine>> vms;
    vms.reserve(100);
    allocationPhase("100_simultaneous_vms",
                    [&]
                    {
                        const long long callsBefore = g_captureCalls;
                        for (int i = 0; i < 100; ++i)
                        {
                            auto item = makeVm();
                            if (!loadBytes(*item, tinyBytecode))
                                throw std::runtime_error("multi-vm load failed");
                            vms.push_back(std::move(item));
                        }
                        for (auto &item : vms)
                            if (!item->callFunction("main"))
                                throw std::runtime_error("multi-vm execution failed");
                        if (g_captured != 17 || g_captureCalls != callsBefore + 100)
                            throw std::runtime_error("multi-vm checksum failed");
                    });
    allocationPhase("release_100_vms", [&] { vms.clear(); });
    return true;
}
#endif
} // namespace

int main()
{
    try
    {
#ifdef KERN_BENCH_MEMORY
        return memoryRun() ? 0 : 1;
#else
        std::cout << "record_type,workload,stage,dimension,min_ns,median_ns,max_ns,ops,source_bytes,assembly_bytes,"
                     "bytecode_bytes,checksum\n";
        for (const int count : {60, 600, 2000})
        {
            const Workload data = prepareWorkload(count);
            if (!speedPipeline(data))
                return 1;
        }
        if (!lookupBenchmark() || !stringBenchmark(16) || !stringBenchmark(1024))
            return 1;
        return 0;
#endif
    }
    catch (const std::exception &error)
    {
        std::cerr << "PerformanceLab: " << error.what() << '\n';
        return 1;
    }
}
