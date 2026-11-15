# KernScript Benchmarks

Dedicated benchmark projects using the public kern API, with optional concrete
VM opcode counts in two existing projects. There are no additional dependencies.
Runtime code is unchanged by the profiling harness.

Every benchmark asserts a CHECKSUM (a known-correct result) before printing
timing: fast+wrong is a failure, exit code 1. The original seven use min-of-7 wall time
(one warmup run, best of 7), reported as ns/op plus ops/s plus the checksum
verdict. Benchmarks are NOT tests: they register no ctest cases.

- CallOverhead: empty `func main() -> void {}` callFunction cost, ns/call.
- StackOps: script loop, 50000 iterations of a compound expression folded
  into a total, ns/iteration.
- ArithLoop: script-side nested `while`, 1000x1000 = 1M accumulation
  iterations, ns/iteration.
- ExternRoundtrip: script loop with one valueless plus one valued extern
  call per iteration, ns/extern-call.
- NestedCalls: recursive fib(20) per run plus a generated 200-deep call
  chain, ns/fib(20) and ns/chain.
- Pipeline: compile+assemble+load of a generated 250-line source
  (60 one-line functions plus a summing main), ns/run. The main run itself
  stays outside the stopwatch; its output is verified before and after.
- RealProgram: scripted mix (iterative factorial, triangular sum, gcd,
  recursive fib), ns/program. The anti-microbenchmark anchor.
- PerformanceLab: separate direct compilation, assembly emission, assembly
  parsing, bytecode loading, and complete direct/text pipelines at 60, 600,
  and 2,000 generated functions. Also measures first/middle/last entry calls
  with 10, 100, and 1,000 functions, and host string roundtrips at 16/1,024 bytes.
- PerformanceMemory: the same public API workloads with a separate allocation
  counter, reporting allocation calls, total requested bytes, additional peak
  live bytes, and retained bytes for each phase. Includes VM creation, loading,
  first/warm execution, clearing, destruction, and 100 simultaneous tiny VMs.

Build: `cmake --build build --config Release --target <name>`, run
`build/Benchmarks/<name>/Release/<name>.exe`. Release measurement needs
Release Conan configs: `conan install . -s build_type=Release
--build=missing -o "&:with_benchmarks=True"` (mirror of the Debug install; build/ only, never
committed). Debug builds stay intact and `ctest --test-dir build -C Debug`
covers the suite.

## Reproducing a profile

Build all nine executables in Release with `KERNSCRIPT_BUILD_BENCHMARKS=ON`.
For an existing configured build:

```powershell
cmake --build build/redesign-release --config Release --parallel 1 --target CallOverhead StackOps ArithLoop ExternRoundtrip NestedCalls Pipeline RealProgram PerformanceLab PerformanceMemory
./Benchmarks/Run-Benchmarks.ps1 -BuildDirectory build/redesign-release -OutputDirectory build/performance-results/2026-11-15 -Rounds 5
```

The Windows runner executes one child process at a time and rotates their
order between rounds. It saves stdout/stderr, CSV results, the engine commit,
working tree status, compiler flags, binary hashes, and machine information.
It also records OS peak working set and private bytes sampled about every
10 ms. Short private-memory peaks can be missed. Paths and generated files
stay inside the project. Run it without simultaneous builds or tests.

The original seven executables report the best of seven samples after one
warmup. PerformanceLab reports minimum/median/maximum of nine samples after
one warmup. Stream construction/reset, compiler/assembler/VM construction,
and output checks are outside isolated stage timers; operation-owned
temporaries and their destruction are included. Repeated load and complete
pipeline measurements include replacing the previously loaded program.
Every timed sample has a correctness check. Entry calls include success
counting. String timing includes the host producer, native byte checksum,
VM loop, and bindings, so it is not a pure string-copy cost.

PerformanceLab emits CSV on stdout. Times are nanoseconds per reported
operation; `ops` is the number of operations in the measured batch. For
generated programs, `dimension` is the helper function count (there is also
one main); for entry lookup it is the function count; for strings it is the
iteration count. Source, assembly, and serialized bytecode sizes are reported
where applicable. Failed compilation, loading, execution, checksums, or
allocation-counter self-checks produce a nonzero exit code. Benchmarks are
not registered as CTest timing thresholds.

PerformanceMemory replaces C++ allocation functions only in its own
single-threaded executable. It measures requested `new`/`new[]` payloads,
including those from statically linked engine code. It excludes allocation
headers, alignment padding, CRT heap overhead, direct `malloc` users, DLL
internals, and OS memory. The separate process figures include the
executable and runtime. Memory phases use fresh output streams, while speed
samples use warmed streams; their allocation totals are not interchangeable.
Do not use the allocation-instrumented executable's speed as a runtime result.

Optional opcode profiles:

```powershell
./build/redesign-release/Benchmarks/ExternRoundtrip/Release/ExternRoundtrip.exe --profile
./build/redesign-release/Benchmarks/NestedCalls/Release/NestedCalls.exe --profile
```

Opcode counts describe executed work. Per-opcode clock probes substantially
perturb time, especially cheap instructions, so their time shares are not
native CPU samples or reliable optimization speedup predictions. Measure
speed without these probes or AddressSanitizer. No machine power settings,
process priority, or CPU affinity are changed by the runner.

## Current 1.0 baseline: 2026-11-15

Engine commit: `d2239e11fe6d3cb76978a86634704cdcbcf18ca5`. Only benchmark
infrastructure was added for this measurement; no engine optimization was
applied. Windows 10.0.19045 x64, 24 logical processors, processor identifier
`AMD64 Family 23 Model 113 Stepping 0, AuthenticAMD`. MSVC 19.51.36252.0,
C++23, Release `/O2 /Ob2 /DNDEBUG`, no sanitizer. The user continued using
the PC, so these are development-machine baselines, not latency guarantees.

Five fresh process rounds were run sequentially with rotating workload
order. The following table reports the median of the five process results;
the range is the lowest/highest process result. Each original process result
is itself the best of seven warmed samples.

| Existing workload | Median | Process-result range |
| --- | ---: | ---: |
| Empty entry call | 41.66 ns/call | 40.75 to 43.96 ns |
| Stack expression loop | 1.089 us/iteration | 1.064 to 1.139 us |
| Arithmetic, 1 million inner iterations | 500.78 ms/run | 493.86 to 526.51 ms |
| Host callbacks, including script loop work | 343.33 ns/callback | 338.15 to 360.05 ns |
| Recursive fib(20) | 11.768 ms/run | 11.441 to 12.331 ms |
| 200-deep call chain | 25.442 us/chain | 25.082 to 27.185 us |
| Original 250-line text compile/assemble/load | 1.043 ms/run | 1.013 to 1.429 ms |
| Mixed scripted program | 11.736 ms/run | 11.607 to 12.575 ms |

Expanded stage results below are medians of the five process medians. Each
process median contains nine warmed samples. Stages run independently, so
their medians need not add exactly to complete-pipeline medians.

| Stage | 60 helpers | 600 helpers | 2,000 helpers |
| --- | ---: | ---: | ---: |
| Direct source to bytecode | 0.713 ms | 7.287 ms | 25.900 ms |
| Source to assembly text | 0.753 ms | 7.223 ms | 26.147 ms |
| Assembly text to bytecode | 0.550 ms | 5.259 ms | 16.642 ms |
| Load and verify bytecode | 0.072 ms | 0.796 ms | 2.393 ms |
| Complete direct compile/load | 0.791 ms | 7.662 ms | 27.567 ms |
| Complete text compile/assemble/load | 1.354 ms | 13.081 ms | 45.001 ms |
| Source bytes | 3,532 | 36,352 | 124,752 |
| Assembly bytes | 4,828 | 48,988 | 166,788 |
| Serialized bytecode bytes | 2,957 | 29,377 | 98,977 |

The already-supported direct compilation path took about 39 to 42% less
time than the text path on these matching generated workloads. This compares
two existing API paths, not a newly implemented optimization. Compilation
scales roughly with this generated workload's size; imports, long expressions,
and other source shapes were not characterized here.

| Entry position | 10 functions | 100 functions | 1,000 functions |
| --- | ---: | ---: | ---: |
| First | 43.90 ns | 44.35 ns | 42.94 ns |
| Middle | 60.14 ns | 220.75 ns | 1,654.24 ns |
| Last | 77.77 ns | 389.38 ns | 3,349.65 ns |

These are complete empty-function calls, including lookup and success
counting. Last-entry calls at 1,000 functions were about 78 times the first
entry's cost. The runtime currently scans the function vector by name for
each public call, consistent with the observed scaling. At 1,000 functions,
the five process medians ranged from 3.250 to 3.677 us for the last entry.

The host-string workload took 1.063 us/iteration at 16 bytes and
1.374 us/iteration at 1,024 bytes. Both perform one host-produced string,
one script local assignment, one native sink/checksum, and a script loop
iteration. These are complete roundtrips rather than isolated binding costs.

Allocation results were identical across all five processes. Numbers below
are C++ requested payloads, with phase inputs/stream construction excluded.
The numeric memory workload has 600 helpers plus main.

| Memory phase | Retained change | Additional peak | Allocation calls | Cumulative requested |
| --- | ---: | ---: | ---: | ---: |
| Direct source to bytecode | 32,807 B | 2,017,483 B | 17,026 | 5,620,047 B |
| Source to assembly text | 65,575 B | 2,017,483 B | 17,027 | 5,685,622 B |
| Assembly text to bytecode | 32,807 B | 1,084,175 B | 10,933 | 2,712,796 B |
| Empty VM construction | 592 B | 592 B | 4 | 592 B |
| Load 29,377 B of bytecode | 129,934 B | 187,909 B | 3,610 | 255,109 B |
| First numeric execution | 160 B | 192 B | 5 | 224 B |
| 100 warm numeric executions | 0 B | 0 B | 0 | 0 B |
| Clear numeric script | -129,934 B | 0 B | 0 | 0 B |
| 1,000 warm 16 B string roundtrips | 0 B | 224 B | 4,000 | 160,000 B |
| 1,000 warm 1,024 B string roundtrips | 0 B | 3,248 B | 4,000 | 3,184,000 B |
| Create/load/run 100 tiny VMs | 94,400 B | 94,400 B | 1,900 | 133,900 B |
| Release those 100 VMs | -94,400 B | 0 B | 0 | 0 B |

The 100-VM pointer vector is reserved before the measured phase. The empty
VM row excludes host registration. Numeric script clearing releases its
program storage while retaining stack capacities and host bindings until VM
destruction. The string workload shows allocation churn, with no retained
growth during the measured warm batches. This is not an exhaustive leak test.

The original benchmark processes peaked at 3.97 to 4.65 MiB of OS working
set. The expanded speed process peaked at 15.07 MiB, with 12.20 MiB maximum
sampled private bytes. These process figures include the harness, compiler
scratch, standard library, executable, and runtime; they are not VM-only
footprints. The allocation-instrumented process has different heap overhead
and should not be used to infer production process memory.

Separate opcode-instrumented runs counted 1,000,015 instructions per host
workload main call, containing 100,001 native calls including its final
capture. Recursive fib(20) executed 339,308 instructions. Stack pushes,
local stores, and temporary Boolean construction account for much of the
executed work. The existing lowering code materializes comparison results
before testing them again in `if`/`while`. Probe time shares were deliberately
not used as native CPU attribution.

At this baseline, the proposed optimization experiments were:

1. Index public entry lookup, checking both call speed and the added memory
   cost at small/large function counts. This has the clearest measured scaling
   problem and does not require changing script semantics.
2. Reduce copies/allocations across string host bindings. The observed four
   allocations per roundtrip include host producer and sink behavior, so split
   those contributions before choosing an ownership/API change.
3. Lower branch conditions directly where semantics permit, then compare
   instruction counts and uninstrumented speed on the arithmetic, recursion,
   and callback workloads. Preserve numeric/NaN behavior and error checks.
4. Investigate compilation allocation churn using the stage baseline. A
   36 KB source requests 5.62 MB cumulatively across 17,026 allocations, while
   peaking at 2.02 MB. Use native allocation/call-stack attribution before
   selecting pooling or container changes.

There is no evidence here that warm numeric execution needs an allocator
redesign. These workloads do not establish whole-application GUI latency,
cross-platform results, cold-start disk performance, or multi-thread scaling.
Raw logs, CSVs, binary hashes, and machine metadata are in
`build/performance-results/2026-11-15/` (ignored build artifacts).

Validation: all nine Release executables completed five process rounds with
successful checksums. Both new profiling executables also completed under
AddressSanitizer, and all 25 CTest checks passed in Release and AddressSanitizer.
The runner's timing/allocation CSV exports and missing-executable failure
were exercised. Formatting and whitespace checks passed. No engine, sample,
or functional-test source was changed for this baseline.

## First bounded optimization pass: 2026-11-15

The retained change accelerates public entry lookup using sorted function
indices. The function table and bytecode call targets keep their existing
order. First-function calls have a direct check; programs with at most 16
functions keep linear lookup. Larger programs build the index only if it
fits the unused portion of `maxProgramBytes`, otherwise they keep linear
lookup too. Clearing, replacing, failing a load, and moving a VM preserve
the existing behavior. Lookup data is placed after the hot runtime members.
Public interfaces, bytecode format, instruction behavior, and string handling
are unchanged.

Comparison method: the original nine Release executables were preserved
before implementation and checked against the binary hashes of the baseline
above. Five matched before/after rounds alternated build order: before/after,
then after/before, and so on. No builds or tests ran alongside measurements.
Both builds used the same compiler/settings and benchmark source. Results
below are medians across process results, using the same internal sampling
methods documented above. They describe this Windows development PC.

| Entry case | Before | After | Speedup |
| --- | ---: | ---: | ---: |
| First of 1,000 | 42.72 ns | 42.59 ns | Within noise |
| Middle of 100 | 209.33 ns | 94.08 ns | 2.23x |
| Last of 100 | 381.98 ns | 98.06 ns | 3.90x |
| Middle of 1,000 | 1,759.75 ns | 121.47 ns | 14.49x |
| Last of 1,000 | 3,341.86 ns | 116.34 ns | 28.72x |
| Middle of 10 | 59.71 ns | 63.46 ns | 3.75 ns slower |
| Last of 10 | 78.09 ns | 82.73 ns | 4.64 ns slower |

An initial always-indexed experiment slowed the middle-of-10 case to about
82 ns. Keeping short lists linear avoided most of that cost. The final
small-list measurements retain a roughly 4 ns overhead, which is disclosed
rather than treated as a gain.

| Other workload | Before | After | Time change |
| --- | ---: | ---: | ---: |
| Empty call | 43.48 ns | 44.10 ns | +1.4% |
| Stack expression iteration | 1.078 us | 1.120 us | +3.9% |
| One million arithmetic iterations | 502.93 ms | 499.85 ms | -0.6% |
| Host callback workload | 344.66 ns/callback | 345.71 ns/callback | +0.3% |
| 200-deep call chain | 25.720 us | 24.915 us | -3.1% |
| Recursive fib(20) | 11.791 ms | 11.532 ms | -2.2% |
| Original text pipeline | 1.093 ms | 1.052 ms | -3.7% |
| Mixed scripted program | 11.883 ms | 11.592 ms | -2.4% |

These changes are all below the existing 5% interpretation threshold and
are not claimed as improvements or regressions. String workload times also
stayed within that range; their allocation counts remain unchanged.

Building the index adds load work. For 600 helpers plus main, isolated
loading increased from 0.727 to 0.844 ms, about 16% or 0.117 ms. The matching
complete direct compile/load measurement was 7.924 versus 7.863 ms, within
noise. At 2,000 helpers, isolated loading increased from 2.379 to 2.650 ms,
while complete direct compile/load was 28.391 versus 28.290 ms. This is a
tradeoff for repeated entry calls, not a claim that every operation got faster.

Memory counts were stable across the five matched rounds:

- The fixed VM overhead increased by 24 B on this x64 build. Empty VM
  construction requested 616 B instead of 592 B, with the same allocation count.
- An indexed program adds four bytes per function and one allocation. The
  601-function loaded program retained 132,338 B instead of 129,934 B, an
  increase of 2,404 B (about 1.85%). Its measured load-phase additional peak
  remained 187,909 B. Index contents count against the existing program budget.
- Clearing scripts releases the index as well as the loaded program. The
  exact-budget regression confirms programs still load when no index fits.
- The 100 tiny VMs retained 96,800 B instead of 94,400 B. They have no index;
  the increase is the fixed object overhead, and all of it is released.
- The 100 warmed numeric executions still made zero allocations. String
  roundtrips still made four allocations each, with no measured warm growth.

The string move experiment was removed after review found a compatibility
risk when optional runtime type information differs between a host binding
and a custom call context. No public callback methods or string ownership
rules were changed. Reducing those allocations is deferred to a separately
scoped API/compatibility decision.

Validation of the retained change: three new regression cases cover shuffled
entry names, internal call targets, signature errors, missing entries, reloads,
clearing, failed loads, VM moves, and the 64-function exact-budget fallback.
All 25 CTest checks pass in Release and AddressSanitizer: 413 library test
cases and 4,225 assertions, plus application workflows and offscreen layouts.
All nine Release benchmarks completed the matched rounds with valid
checksums; both new profiling executables also passed under AddressSanitizer.
Independent review found no remaining correctness issues in the retained
change. Formatting and whitespace checks pass.

One intermediate sanitizer suite encountered Windows access denied while
LifeLab replaced a temporary script file. Its isolated rerun and the final
full sanitizer suite passed. No file-handling code changed in this pass.
The failed run is preserved in
`build/task-tmp/performance-asan-intermediate-file-access-failure.log`.

Final comparison logs, CSVs, hashes, and summary are in
`build/performance-results/entry-lookup-final-2026-11-15/`. Earlier experimental
measurements remain under `build/performance-results/entry-lookup-2026-11-15/`
and are not the final results. All comparison binaries and outputs are
ignored project-local build artifacts.

## Historical 0.1 baseline (Release)

These measurements predate the 1.0 verified call contract and are retained
for reference. Re-run the migrated benchmarks for current performance; the
old timings are not a regression threshold for the new runtime.

Date: 2026-10-13. Commit: e02e30e (scaffolding+benches; this file came
second and changes no code, so the numbers stand for it too).
Machine: AMD Ryzen 9 3900X 12-Core, 16 GB RAM, Windows 10 Pro x64,
24 hardware threads. MSVC v145, Visual Studio 18 2026 generator.

| Bench | Result (min of 7) | Checksum |
| --- | --- | --- |
| CallOverhead | 15.633 ns/call | callFunction true, 800001/800001 calls |
| StackOps | 564.655 ns/iteration | total 1249975000 (50000-iteration triangular sum) |
| ArithLoop | 242.82 ns/iteration | total 500500000 (1000 x 500500) |
| ExternRoundtrip | 176.895 ns/extern-call | 100000/100000 host calls per kind, acc 1250025000 |
| NestedCalls/fib20 | 5.641 ms/run | fib(20) = 6765 |
| NestedCalls/chain200 | 13.856 us/chain | 200-deep chain = 200 |
| Pipeline | 0.836 ms/run (250 lines) | loads OK, main output 1770 |
| RealProgram | 6.071 ms/program | fact 479001600, sum 500500, gcd 6, fib 6765 |

Two deliberate deviations from the nominal contracts, both forced by the
documented 32-bit range (out-of-range results fail the run instead of
wrapping):

- ArithLoop accumulates in a 1000x1000 nest (1M inner iterations) for a
  total of 500500000. A flat 1M loop summing i would need N*(N+1)/2 =
  500000500000, far past the 32-bit range.
- RealProgram computes factorial(12) = 479001600, the largest factorial
  that fits (13! = 6227020800 overflows). A factorial(20) needs 63 bits,
  so no 32-bit script loop can produce its exact digits.

ArithLoop raises maxExecutionSteps to 100M (about a dozen instructions x
1M iterations passes the default 10M budget); every other bench uses the
default VM limits.

## Noisiness caveats

Dev box, not a lab machine: background load, boost clocks and thermal
state all move these numbers. Min-of-7 suppresses one-sided OS noise but
not bias. Treat deltas under 5% as meaningless; re-run the suite on the
same machine before claiming a regression or an improvement. Checksums
are exact on every run: any FAIL (or a TIMER SUSPECT line, i.e. a
non-positive ns/op) is a harness or engine problem to investigate, never
a result to celebrate.
