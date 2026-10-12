# KernScript Benchmarks

Umbrella of one executable per stage, all through the public kern API only
(like the Samples). Shared header-only harness: `common/bench.h` (chrono
min-of-N timer, result/checksum print, machine note). No third-party deps.

Every benchmark asserts a CHECKSUM (a known-correct result) before printing
timing: fast+wrong is a failure, exit code 1. Timing is min-of-7 wall time
(one warmup run, best of 7), reported as ns/op plus ops/s plus the checksum
verdict. Benchmarks are NOT tests: they register no ctest cases.

- CallOverhead: empty `func main() {}` callFunction cost, ns/call.
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

Build: `cmake --build build --config Release --target <name>`, run
`build/Benchmarks/<name>/Release/<name>.exe`. Release measurement needs
Release Conan configs: `conan install . -s build_type=Release
--build=missing` (mirror of the Debug install; build/ only, never
committed). Debug builds stay intact and `ctest --test-dir build -C Debug`
covers the suite.

## Baseline (Release)

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
