#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <kern/Kern.h>

#include "../../common/bench.h"

namespace
{

// main runs per timing repeat.
constexpr int kRuns = 5;

// Iterative factorial(12): 12 is the largest n whose factorial fits the
// 32-bit range (12! = 479001600; 13! = 6227020800 overflows and the engine
// correctly fails the run instead of wrapping). A factorial(20) needs 63
// bits, so no 32-bit script loop can produce its exact digits.
constexpr int kExpectedFact = 479001600;
// Triangular sum 1..1000.
constexpr int kExpectedSum = 500500;
// gcd(48, 18) by subtraction.
constexpr int kExpectedGcd = 6;
// Recursive fib(20).
constexpr int kExpectedFib = 6765;

const std::vector<int> kExpected = {kExpectedFact, kExpectedSum, kExpectedGcd, kExpectedFib};

std::vector<int> g_captured;

void put(int value) { g_captured.push_back(value); }

const std::string kSource = R"(extern put(x);
func fact(n) {
  var acc = 1;
  var k = n;
  while (k > 1) {
    acc = acc * k;
    k = k - 1;
  }
  return acc;
}
func gcd(a, b) {
  var x = a;
  var y = b;
  while (x != y) {
    if (x > y) {
      x = x - y;
    } else {
      y = y - x;
    }
  }
  return x;
}
func fib(n) {
  if (n <= 1) {
    return n;
  }
  var r = fib(n - 1);
  var s = fib(n - 2);
  return r + s;
}
func main() {
  put(fact(12));
  var s = 0;
  var i = 1;
  while (i <= 1000) {
    s = s + i;
    i = i + 1;
  }
  put(s);
  put(gcd(48, 18));
  put(fib(20));
}
)";

} // namespace

int main()
{
    std::cout << "machine: " << bench::machineNote() << '\n';

    std::unique_ptr<kern::IVirtualMachine> vm = bench::makeVm();
    vm->addFunction("put", kern::createExternFunction(&put));
    if (!bench::compileLoad(kSource, *vm))
    {
        std::cerr << "RealProgram: pipeline failed\n";
        return 1;
    }

    // CHECKSUM: the exact captured sequence, verified before timing.
    try
    {
        if (!vm->callFunction("main"))
        {
            std::cerr << "RealProgram: checksum FAIL, main not found\n";
            return 1;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "RealProgram: checksum FAIL, " << error.what() << '\n';
        return 1;
    }
    if (g_captured != kExpected)
    {
        std::cerr << "RealProgram: checksum FAIL\n";
        return 1;
    }

    const double nsPerProgram = bench::minNsPerOp(
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
                    // Counted as missing below: the capture count falls short.
                }
            }
        },
        kRuns);

    // Every run appends exactly kExpected.size() values, so the total count
    // plus the repeating exact sequence proves all timed work is real.
    const long long mains = 1 + static_cast<long long>(kRuns) * (1 + bench::kRepeats);
    bool checksumOk = (static_cast<long long>(g_captured.size()) == mains * 4);
    for (long long i = 0; checksumOk && i < static_cast<long long>(g_captured.size()); ++i)
    {
        if (g_captured[static_cast<std::size_t>(i)] != kExpected[static_cast<std::size_t>(i % 4)])
        {
            checksumOk = false;
        }
    }
    return bench::report("RealProgram", nsPerProgram, kRuns, checksumOk);
}
