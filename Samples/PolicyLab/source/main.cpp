#include <cmath>
#include <iostream>

#include "Policy.h"

namespace
{
using namespace policylab;
fs::path bundled(const fs::path &executable)
{
    for (const auto &directory :
         {executable.parent_path() / "policies", fs::path("policies"), fs::path("Samples/PolicyLab/policies")})
        if (fs::is_regular_file(directory / "standard.ks"))
            return fs::absolute(directory);
    return {};
}
void require(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejected(F operation, const char *message)
{
    bool failed = false;
    try
    {
        operation();
    }
    catch (const std::exception &)
    {
        failed = true;
    }
    require(failed, message);
}
int verify(const fs::path &policies, const fs::path &testRoot)
{
    const auto directory = samples::testDirectory(testRoot);
    const auto orders = readOrders(policies / "orders.csv");
    Policy standard(policies / "standard.ks"), weekend(policies / "weekend.ks");
    const auto before = orders;
    const auto a = standard.price(orders), b = weekend.price(orders);
    require(a.size() == 10 && orders == before, "The bundled batch changed its inputs");
    require(a[0].discount == 600 && a[0].shipping == 500 && a[0].tax == 2380 && a[0].total == 14280 &&
                a[1].total == 19800 && a[2].total == 7320 && a[3].total == 6000,
            "The baseline quotes differ from the cent arithmetic");
    require(b[0].total == 13560 && b[1].shipping == 0 && b[1].total == 19200 && b[3].total == 5400,
            "The weekend comparison did not apply its discount or free delivery");
    require(a[4].discount == 0 && a[5].discount == 500 && a[8].tax == 1 && a[8].total == 2 && a[9].total == 0 &&
                rateCents(1, 4999) == 0 && rateCents(1, 5000) == 1 && rateCents(200000000, 10000) == 200000000,
            "Thresholds, zero orders, rounding or wide arithmetic failed");
    require(a[0].explanation.contains("5 percent") && b[1].explanation.contains("free delivery"),
            "The policy explanation is missing");
    const auto output = directory / "comparison.csv";
    const auto comparison = report(orders, a, b);
    writeReport(output, comparison, {policies / "orders.csv", policies / "standard.ks"});
    require(samples::readText(output, 1024 * 1024) == comparison &&
                comparison.contains("candidate_total_cents,delta_cents") && comparison.contains(",13560,-720,"),
            "The comparison CSV lost results or differences");
    writeReport(output, report(orders, a), {});
    require(!samples::readText(output, 1024 * 1024).contains("candidate_total_cents"), "Report replacement failed");
    const auto input = directory / "orders.csv";
    const std::string header = "id,tier,subtotal_cents,shipping_cents,tax_bps\n";
    const auto write = [&](const fs::path &file, const std::string &content)
    { samples::writeAtomically(file, [&](std::ostream &out) { out << content; }); };
    for (const auto &invalid :
         {header, header + "A,std,12.5,0,2000\n", header + "A,std,-1,0,2000\n", header + "A,std,100,0,10001\n",
          header + "A,std,100,0,0,extra\n", header + "A,std,100,0,0\nA,vip,100,0,0\n"})
    {
        write(input, invalid);
        rejected([&] { (void)readOrders(input); }, "An invalid input batch was accepted");
    }
    write(input, header + "A,std,100,0,2000\n");
    const auto original = samples::readText(input, 4096);
    rejected([&] { writeReport(input, "wrong", {input}); }, "Report output overwrote its input");
    require(samples::readText(input, 4096) == original, "Output protection damaged the input");
    const auto helper = directory / "helper.ks";
    write(helper, "module helper;\n");
    rejected([&] { writeReport(helper, "wrong", {input}); }, "A report overwrote an imported script");
    require(samples::readText(helper, 4096) == "module helper;\n", "Imported script protection lost source text");
    require(csv("why, \"quoted\"") == "\"why, \"\"quoted\"\"\"", "CSV explanation quoting is invalid");

    const auto broken = directory / "policy.ks";
    const std::string protocol = "extern emit(d,s,t) -> void; extern explain(t) -> void; ";
    const auto previousReport = samples::readText(output, 1024 * 1024);
    for (const auto &source : {std::string("func price() -> void { var broken = ; }"),
                               std::string("func another() -> void {}"), std::string("func price() -> void {}"),
                               protocol + "func price() -> void { emit(0,0,0); emit(0,0,0); explain(\"duplicate\"); }",
                               protocol + "func price() -> void { emit(0,0,0); }",
                               protocol + "func price() -> void { emit(-1,0,0); explain(\"invalid\"); }",
                               protocol + "func price() -> void { var zero=0; var x=1/zero; }",
                               protocol + "extern subtotal_cents() -> value; func price() -> void { "
                                          "if (subtotal_cents()==20000) { var zero=0; var x=1/zero; } "
                                          "emit(0,0,0); explain(\"first order succeeds\"); }"})
    {
        write(broken, source);
        rejected(
            [&]
            {
                Policy policy(broken);
                const auto result = policy.price(orders);
                writeReport(output, report(orders, result), {input, broken});
            },
            "An invalid policy produced a batch report");
        require(active == nullptr && samples::readText(output, 1024 * 1024) == previousReport &&
                    standard.price(orders) == a,
                "A failed batch changed existing output or the working policy");
    }
    std::ostringstream batch;
    batch << header;
    for (int i = 0; i < 2000; ++i)
        batch << "B-" << i << ','
              << (i % 3 == 0   ? "vip"
                  : i % 3 == 1 ? "staff"
                               : "std")
              << ',' << (i * 137 % 100000) << ",500,2000\n";
    write(input, batch.str());
    const auto larger = readOrders(input);
    const auto largerA = standard.price(larger), largerB = weekend.price(larger);
    require(largerA.size() == 2000 && largerA == standard.price(larger), "Larger batch results are not stable");
    long long changed = 0;
    for (std::size_t i = 0; i < larger.size(); ++i)
    {
        const auto &order = larger[i];
        const int rate = order.tier == "staff"     ? 3000
                         : order.tier == "vip"     ? (order.subtotal >= 10000 ? 2000 : 1000)
                         : order.subtotal >= 10000 ? 500
                                                   : 0;
        const int discount = static_cast<int>((static_cast<long long>(order.subtotal) * rate + 5000) / 10000);
        const int base = order.subtotal - discount + order.shipping;
        const int tax = static_cast<int>((static_cast<long long>(base) * order.taxBps + 5000) / 10000);
        require(largerA[i].discount == discount && largerA[i].tax == tax && largerA[i].total == base + tax,
                "A larger quote differs from the independent integer calculation");
        changed += largerA[i].total != largerB[i].total;
    }
    require(changed > 1000, "The larger policy comparison did not change realistic orders");
    std::cout << "PolicyLab verify: PASS (cent rounding, explained quotes, input validation, atomic reports, "
                 "2000-order comparison)\n";
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        fs::path input, policy, compare, output, testRoot = "build/test-tmp/PolicyLab";
        bool runVerify = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help")
            {
                std::cout << "PolicyLab: batch pricing and explained policy comparison in integer cents.\n"
                             "PolicyLab [--input orders.csv] [--policy policy.ks] [--compare alternative.ks] [--output "
                             "quotes.csv]\n"
                             "PolicyLab --verify [--test-root DIR]\n";
                return 0;
            }
            if (arg == "--verify")
            {
                runVerify = true;
                continue;
            }
            if ((arg == "--input" || arg == "--policy" || arg == "--compare" || arg == "--output" ||
                 arg == "--test-root") &&
                i + 1 < argc)
            {
                const fs::path value = argv[++i];
                if (arg == "--input")
                    input = value;
                else if (arg == "--policy")
                    policy = value;
                else if (arg == "--compare")
                    compare = value;
                else if (arg == "--output")
                    output = value;
                else
                    testRoot = value;
            }
            else
                throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }
        const auto resources = bundled(argc > 0 ? fs::path(argv[0]) : fs::path{});
        if (runVerify)
        {
            if (resources.empty())
                throw std::runtime_error("The bundled PolicyLab policies were not found");
            return verify(resources, testRoot);
        }
        if (input.empty() || policy.empty())
        {
            if (resources.empty())
                throw std::runtime_error("Bundled inputs were not found. Supply --input and --policy");
            if (input.empty())
                input = resources / "orders.csv";
            if (policy.empty())
                policy = resources / "standard.ks";
        }
        input = fs::absolute(input);
        policy = fs::absolute(policy);
        const auto orders = readOrders(input);
        Policy baseline(policy);
        const auto quotes = baseline.price(orders);
        std::vector<Quote> alternative;
        std::vector<fs::path> inputs{input, policy};
        if (!compare.empty())
        {
            compare = fs::absolute(compare);
            Policy candidate(compare);
            alternative = candidate.price(orders);
            inputs.push_back(compare);
        }
        const auto text = report(orders, quotes, alternative);
        if (output.empty())
        {
            std::cout << text;
            if (!std::cout)
                throw std::runtime_error("Could not finish writing the report");
        }
        else
            writeReport(output, text, inputs);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "PolicyLab: " << error.what() << '\n';
        return 1;
    }
}
