#pragma once

#include <charconv>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

#include "../../Support/Files.h"

namespace policylab
{
namespace fs = std::filesystem;
constexpr int MaxMoney = 100000000;
struct Order
{
    std::string id, tier;
    int subtotal = 0, shipping = 0, taxBps = 0;
    bool operator==(const Order &) const = default;
};
struct Quote
{
    int discount = 0, shipping = 0, tax = 0, total = 0;
    std::string explanation;
    bool operator==(const Quote &) const = default;
};

inline void identifier(const std::string &name)
{
    if (name.empty() || name.size() > 64)
        throw std::runtime_error("An order ID or tier must contain 1 through 64 characters");
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            throw std::runtime_error("Order IDs and tiers use letters, digits, underscores and hyphens");
}
inline int integer(const std::string &text, int maximum)
{
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value < 0 || value > maximum)
        throw std::runtime_error("Invalid nonnegative integer: " + text);
    return value;
}
inline std::vector<Order> readOrders(const fs::path &path)
{
    std::istringstream input(samples::readText(path, 4 * 1024 * 1024));
    std::string line;
    auto getLine = [&]
    {
        if (!std::getline(input, line))
            return false;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        return true;
    };
    if (!getLine() || line != "id,tier,subtotal_cents,shipping_cents,tax_bps")
        throw std::runtime_error("Expected the PolicyLab orders CSV header");
    std::set<std::string> ids;
    std::vector<Order> orders;
    int lineNumber = 1;
    while (getLine())
    {
        ++lineNumber;
        try
        {
            std::vector<std::string> cells;
            std::size_t begin = 0, end;
            while ((end = line.find(',', begin)) != std::string::npos)
            {
                cells.push_back(line.substr(begin, end - begin));
                begin = end + 1;
            }
            cells.push_back(line.substr(begin));
            if (cells.size() != 5)
                throw std::runtime_error("Expected five columns");
            Order order{cells[0], cells[1], integer(cells[2], MaxMoney), integer(cells[3], MaxMoney),
                        integer(cells[4], 10000)};
            identifier(order.id);
            identifier(order.tier);
            if (!ids.insert(order.id).second)
                throw std::runtime_error("Duplicate order ID: " + order.id);
            orders.push_back(std::move(order));
            if (orders.size() > 10000)
                throw std::runtime_error("An input batch supports at most 10000 orders");
        }
        catch (const std::exception &error)
        {
            throw std::runtime_error(path.string() + ":" + std::to_string(lineNumber) + ": " + error.what());
        }
    }
    if (orders.empty())
        throw std::runtime_error("The order batch is empty");
    return orders;
}
inline std::string csv(const std::string &text)
{
    std::string result = "\"";
    for (char c : text)
        result += c == '"' ? std::string("\"\"") : std::string(1, c);
    return result + '"';
}

struct Context
{
    const Order &order;
    Quote quote;
    int emissions = 0, reasons = 0;
};
inline Context *active = nullptr;
class Scope
{
    Context *previous;

public:
    explicit Scope(Context &context) : previous(active) { active = &context; }
    ~Scope() { active = previous; }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
};
inline int subtotal() { return active->order.subtotal; }
inline int shipping() { return active->order.shipping; }
inline int taxBps() { return active->order.taxBps; }
inline std::string tier() { return active->order.tier; }
inline int rateCents(int amount, int basisPoints)
{
    if (amount < 0 || amount > 2 * MaxMoney || basisPoints < 0 || basisPoints > 10000)
        throw std::runtime_error("rate_cents needs a nonnegative cent amount and a rate from 0 to 10000 basis points");
    return static_cast<int>((static_cast<long long>(amount) * basisPoints + 5000) / 10000);
}
inline void emit(int discount, int delivery, int tax)
{
    if (++active->emissions != 1)
        throw std::runtime_error("A policy must emit exactly one quote");
    if (discount < 0 || discount > active->order.subtotal || delivery < 0 || delivery > MaxMoney || tax < 0 ||
        tax > 2 * MaxMoney)
        throw std::runtime_error("The policy emitted invalid cent amounts");
    active->quote.discount = discount;
    active->quote.shipping = delivery;
    active->quote.tax = tax;
    active->quote.total = active->order.subtotal - discount + delivery + tax;
}
inline void explain(std::string reason)
{
    if (reason.empty() || reason.size() > 1024 || ++active->reasons > 16)
        throw std::runtime_error("A policy supports 1 through 16 explanations of at most 1024 bytes");
    if (!active->quote.explanation.empty())
        active->quote.explanation += "; ";
    active->quote.explanation += reason;
}

class Logger final : public kern::ILogger
{
public:
    std::string messages;

protected:
    void onError(const std::string &message) override { messages += message + '\n'; }
};
class Policy
{
    Logger m_logger;
    std::unique_ptr<kern::IVirtualMachine> m_vm;

public:
    explicit Policy(const fs::path &source)
    {
        std::istringstream input(samples::readText(source, 1024 * 1024));
        std::stringstream bytecode;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&m_logger));
        if (!compiler->compileBytecode(input, bytecode, source.string(), {source.parent_path().string()}))
            throw std::runtime_error("Policy compile failed:\n" + m_logger.messages);
        kern::VMConfig config;
        config.maxExecutionSteps = 100000;
        m_vm.reset(kern::createVirtualMachine(&m_logger, config));
        m_vm->addFunction("subtotal_cents", &subtotal);
        m_vm->addFunction("shipping_cents", &shipping);
        m_vm->addFunction("tax_bps", &taxBps);
        m_vm->addFunction("tier", &tier);
        m_vm->addFunction("rate_cents", &rateCents);
        m_vm->addFunction("emit", &emit);
        m_vm->addFunction("explain", &explain);
        if (!m_vm->load(bytecode))
            throw std::runtime_error("Policy bytecode could not be loaded");
    }
    Quote price(const Order &order)
    {
        Context context{order};
        Scope scope(context);
        if (!m_vm->callFunction("price"))
            throw std::runtime_error("A policy must expose func price() -> void");
        if (context.emissions != 1 || context.reasons == 0)
            throw std::runtime_error("A policy must emit once and explain its result");
        return context.quote;
    }
    std::vector<Quote> price(const std::vector<Order> &orders)
    {
        std::vector<Quote> quotes;
        for (const auto &order : orders)
        {
            try
            {
                quotes.push_back(price(order));
            }
            catch (const std::exception &error)
            {
                throw std::runtime_error("Order " + order.id + ": " + error.what());
            }
        }
        return quotes;
    }
};

inline std::string report(const std::vector<Order> &orders, const std::vector<Quote> &baseline,
                          const std::vector<Quote> &candidate = {})
{
    std::ostringstream out;
    out << "id,tier,subtotal_cents,discount_cents,shipping_cents,tax_cents,total_cents,explanation";
    if (!candidate.empty())
        out << ",candidate_total_cents,delta_cents,candidate_explanation";
    out << '\n';
    for (std::size_t i = 0; i < orders.size(); ++i)
    {
        const auto &order = orders[i];
        const auto &quote = baseline.at(i);
        out << order.id << ',' << order.tier << ',' << order.subtotal << ',' << quote.discount << ',' << quote.shipping
            << ',' << quote.tax << ',' << quote.total << ',' << csv(quote.explanation);
        if (!candidate.empty())
            out << ',' << candidate.at(i).total << ',' << candidate.at(i).total - quote.total << ','
                << csv(candidate.at(i).explanation);
        out << '\n';
    }
    return out.str();
}
inline bool sameFile(const fs::path &a, const fs::path &b)
{
    return fs::weakly_canonical(a) == fs::weakly_canonical(b) ||
           (fs::exists(a) && fs::exists(b) && fs::equivalent(a, b));
}
inline void writeReport(const fs::path &path, const std::string &text, const std::vector<fs::path> &inputs)
{
    samples::requireDataDestination(path);
    for (const auto &input : inputs)
        if (sameFile(path, input))
            throw std::runtime_error("Choose a report path different from the input orders and policy scripts");
    samples::writeAtomically(path, [&](std::ostream &out) { out << text; });
}
} // namespace policylab
