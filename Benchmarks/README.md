# KernScript benchmarks

The benchmark executables exercise the public KernScript API and validate
results with checksums. Failed operations or checksums return a nonzero exit
code. Benchmarks are separate from CTest and do not impose timing thresholds.

## Workloads

| Executable | Workload | Reported operation |
| --- | --- | --- |
| CallOverhead | Repeated calls to an empty script entry | Entry call |
| StackOps | Compound arithmetic expressions in a 50,000-iteration loop | Loop iteration |
| ArithLoop | Nested 1,000 x 1,000 integer accumulation | Inner iteration |
| ExternRoundtrip | A void and a valued host callback per script loop iteration | Callback, including loop overhead |
| NestedCalls | Recursive fib(20) and a 200-function call chain | Fibonacci run or chain traversal |
| Pipeline | Compile, assemble, and load generated source with 60 helper functions | Complete pipeline |
| RealProgram | Factorial, summation, GCD, and recursive Fibonacci | Complete program |
| PerformanceLab | Separate pipeline stages, entry lookup scaling, and host string transfers | Stage, entry call, or string roundtrip |
| PerformanceMemory | Compilation, VM lifecycle, string transfers, and simultaneous VMs | Allocation totals and retained/peak bytes per phase |

Arithmetic inputs stay within the VM's 32-bit integer range. ArithLoop raises
`maxExecutionSteps` to accommodate its longer loop.

PerformanceLab uses generated programs with 60, 600, and 2,000 helper functions;
entry lookup with 10, 100, and 1,000 functions; and 16-byte and 1,024-byte host
strings. Function position and input size are part of the workload definition.

## Build and run

Use Release dependencies and enable benchmarks:

```powershell
conan install . -s build_type=Release --build=missing -o "&:with_benchmarks=True"
cmake --preset conan-default
cmake --build build --config Release
./Benchmarks/Run-Benchmarks.ps1 -BuildDirectory build -OutputDirectory build/performance-results -Rounds 5
```

For direct CMake configuration, enable `KERNSCRIPT_BUILD_BENCHMARKS`.
Individual executables can be built with `--target <name>`.

With Visual Studio, executables are under
`<build>/Benchmarks/<name>/Release/<name>.exe`. PerformanceMemory shares the
PerformanceLab build directory. Other generators generally omit `Release`.

The PowerShell runner supports Windows Release builds. It runs child processes
sequentially, rotates workload order between rounds, and writes logs, timing
and allocation CSVs, process-memory samples, and build/machine metadata to the
output directory. Build and output paths must stay inside the project.

## Timing methodology

The individual timing workloads use one warmup followed by the best of seven
samples. PerformanceLab uses one warmup and reports the minimum, median, and
maximum of nine samples. Times are normalized to the operation listed above.

PerformanceLab excludes stream setup/reset, compiler/assembler/VM construction,
and correctness checks from isolated stage timings. Operation-owned temporaries
and their destruction are included. Repeated loads include replacing the
previously loaded program. String timings include the producer, native checksum,
VM loop, and bindings, rather than just string copying.

PerformanceLab writes CSV to stdout. `ops` is the batch's operation count;
`dimension` is the helper function count, entry function count, or string loop
iteration count, depending on the workload. Source, assembly, and bytecode sizes
are included where applicable.

Measure without AddressSanitizer, opcode probes, simultaneous builds, or tests.
Use matching compiler settings and workloads for comparisons, repeat runs, and
consider run-to-run variation. Background load, boost clocks, and temperature
can affect results. The runner does not change priority, affinity, or power
settings. Keep generated measurement results in the ignored build directory.

## Memory measurements

PerformanceMemory replaces `new` and `delete` only in its own single-threaded
executable. It reports allocation calls, cumulative requested bytes, change in
live requested bytes, and peak live bytes above each phase's starting point.

These figures include C++ payloads from statically linked engine code, but
exclude allocation headers, alignment padding, heap overhead, direct `malloc`
users, DLL internals, and OS memory. Inputs and stream construction are outside
measured allocation phases. Memory phases use fresh output streams; timing
samples use warmed streams. Their allocation totals are not interchangeable.
Do not use the allocation-instrumented executable for speed comparisons.

The runner separately records process peak working set and samples private
bytes about every 10 ms. These include the executable, runtime, and harness;
short private-memory peaks may be missed.

## Opcode profiles

```powershell
./build/Benchmarks/ExternRoundtrip/Release/ExternRoundtrip.exe --profile
./build/Benchmarks/NestedCalls/Release/NestedCalls.exe --profile
```

These optional profiles use the concrete VM implementation. Counts describe
executed instructions. Per-instruction clock probes perturb timings, especially
for cheap instructions; their time shares are not native CPU samples or reliable
speedup predictions. Use uninstrumented runs to measure performance.
