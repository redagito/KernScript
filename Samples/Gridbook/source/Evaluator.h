#pragma once

#include "../../Support/Numbers.h"
#include "Workbook.h"
#include <cmath>
#include <iomanip>
#include <kern/Kern.h>
#include <locale>
#include <map>
#include <memory>
#include <numeric>
#include <utility>

namespace gridbook
{
struct Result
{
    bool number = false;
    double value = 0;
    std::string display, error;
};

class Diagnostics : public kern::ILogger
{
    void onError(const std::string &message) override { messages += message + '\n'; }

public:
    std::string messages;
};

inline std::string format(double value)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(7) << value;
    return out.str();
}

class Evaluator
{
    struct Plan
    {
        std::string text, expression, error;
        std::vector<int> dependencies;
        std::vector<std::vector<int>> ranges;
        std::unique_ptr<kern::IVirtualMachine> vm;
    };
    enum class Operation
    {
        Cell,
        Emit,
        RangeCount,
        RangeValue,
        RangeNumeric
    };
    class Binding : public kern::IExternFunction
    {
        Evaluator &m_owner;
        Operation m_operation;

    public:
        Binding(Evaluator &owner, Operation op) : m_owner(owner), m_operation(op) {}
        kern::FunctionSignature getSignature() const override
        {
            const unsigned arity =
                m_operation == Operation::RangeValue || m_operation == Operation::RangeNumeric ? 2 : 1;
            return {arity, m_operation == Operation::Emit ? kern::ReturnKind::Void : kern::ReturnKind::Value};
        }
        bool call(kern::IExternCall &call) override { return m_owner.invoke(m_operation, call); }
    };
    std::vector<Plan> m_plans;
    std::vector<Result> m_results;
    const Data *m_data = nullptr;
    Plan *m_active = nullptr;
    std::vector<std::string> m_paths;
    int m_rows = 0, m_columns = 0;
    bool m_force = true;
    bool m_current = false;
    double m_emitted = 0;
    int m_emitCount = 0;

    static bool letter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
    static bool digit(char c) { return c >= '0' && c <= '9'; }
    static void spaces(const std::string &text, std::size_t &i)
    {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
            ++i;
    }
    static std::string upper(std::string text)
    {
        for (auto &c : text)
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - 'a' + 'A');
        return text;
    }
    static std::string token(const std::string &text, std::size_t &i)
    {
        const auto start = i;
        while (i < text.size() && (letter(text[i]) || digit(text[i])))
            ++i;
        return text.substr(start, i - start);
    }
    Plan parse(const Data &data, const std::string &text)
    {
        Plan plan;
        plan.text = text;
        const std::string expression = text.substr(1);
        std::set<int> dependencies;
        static const std::map<std::string, std::string> aggregates = {{"SUM", "sum"},         {"AVG", "average"},
                                                                      {"AVERAGE", "average"}, {"MIN", "minimum"},
                                                                      {"MAX", "maximum"},     {"COUNT", "count"}};
        for (std::size_t i = 0; i < expression.size();)
        {
            const char c = expression[i];
            if (c == '/' && i + 1 < expression.size() && expression[i + 1] == '/')
            {
                plan.expression += expression.substr(i);
                break;
            }
            else if (c == '"')
            {
                plan.expression += expression[i++];
                bool closed = false;
                while (i < expression.size())
                {
                    const char next = expression[i++];
                    plan.expression += next;
                    if (next == '\\' && i < expression.size())
                        plan.expression += expression[i++];
                    else if (next == '"')
                    {
                        closed = true;
                        break;
                    }
                }
                if (!closed)
                    throw std::runtime_error("Unterminated string in formula");
            }
            else if (digit(c) || (c == '.' && i + 1 < expression.size() && digit(expression[i + 1])))
            {
                const auto start = i;
                while (i < expression.size() && digit(expression[i]))
                    ++i;
                if (i < expression.size() && expression[i] == '.')
                {
                    ++i;
                    while (i < expression.size() && digit(expression[i]))
                        ++i;
                }
                if (i < expression.size() && (expression[i] == 'e' || expression[i] == 'E'))
                {
                    ++i;
                    if (i < expression.size() && (expression[i] == '+' || expression[i] == '-'))
                        ++i;
                    while (i < expression.size() && digit(expression[i]))
                        ++i;
                }
                auto number = expression.substr(start, i - start);
                if (number.front() == '.')
                    number.insert(number.begin(), '0');
                if (number.back() == '.')
                    number += '0';
                if (number.find_first_of(".eE") == std::string::npos)
                    number += ".0";
                float parsedNumber = 0;
                const auto parsed = std::from_chars(number.data(), number.data() + number.size(), parsedNumber,
                                                    std::chars_format::general);
                if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
                    !std::isfinite(parsedNumber))
                    throw std::runtime_error("Invalid or out-of-range numeric literal");
                plan.expression += samples::floatSource(parsedNumber);
            }
            else if (letter(c))
            {
                const auto word = token(expression, i);
                const auto aggregate = aggregates.find(upper(word));
                auto cursor = i;
                spaces(expression, cursor);
                if (aggregate != aggregates.end() && cursor < expression.size() && expression[cursor] == '(')
                {
                    ++cursor;
                    spaces(expression, cursor);
                    const auto first = address(token(expression, cursor));
                    spaces(expression, cursor);
                    auto last = first;
                    if (cursor < expression.size() && expression[cursor] == ':')
                    {
                        ++cursor;
                        spaces(expression, cursor);
                        last = address(token(expression, cursor));
                        spaces(expression, cursor);
                    }
                    if (!first || !last || cursor >= expression.size() || expression[cursor] != ')')
                        throw std::runtime_error("Aggregates accept one cell or rectangular range, such as SUM(A1:B4)");
                    if (!data.contains(*first) || !data.contains(*last))
                        throw std::out_of_range("Range is outside the workbook");
                    std::vector<int> range;
                    for (int r = std::min(first->row, last->row); r <= std::max(first->row, last->row); ++r)
                        for (int col = std::min(first->column, last->column);
                             col <= std::max(first->column, last->column); ++col)
                        {
                            const auto index = data.index({r, col});
                            range.push_back(index);
                            dependencies.insert(index);
                        }
                    plan.expression += "sheet::" + aggregate->second + "(" + std::to_string(plan.ranges.size()) + ")";
                    plan.ranges.push_back(std::move(range));
                    i = cursor + 1;
                }
                else if (auto cell = address(word))
                {
                    if (!data.contains(*cell))
                        throw std::out_of_range("Reference " + word + " is outside the workbook");
                    const auto index = data.index(*cell);
                    dependencies.insert(index);
                    plan.expression += "cell(" + std::to_string(index) + ")";
                }
                else
                {
                    if (word == "cell" || word == "emit" || word.rfind("range_", 0) == 0)
                        throw std::runtime_error(
                            "Use cell addresses and aggregate functions instead of internal host helpers");
                    plan.expression += word;
                }
            }
            else
            {
                if (c == ';' || c == '{' || c == '}' || c == '\n' || c == '\r')
                    throw std::runtime_error("Enter one formula expression");
                plan.expression += c;
                ++i;
            }
        }
        plan.dependencies.assign(dependencies.begin(), dependencies.end());
        return plan;
    }

    std::unique_ptr<kern::IVirtualMachine> compile(const Plan &plan, int index)
    {
        Diagnostics logger;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
        const auto source = "import sheet;\nextern cell(index) -> value;\nextern emit(value) -> void;\n"
                            "func calc() -> void { emit((" +
                            plan.expression + "\n)); }\n";
        std::istringstream input(source);
        std::stringstream bytes;
        const int columns = std::max(1, m_columns);
        if (!compiler->compileBytecode(input, bytes, name({index / columns, index % columns}), m_paths))
            throw std::runtime_error(logger.messages.empty() ? "Could not compile formula" : logger.messages);
        kern::VMConfig config;
        config.maxExecutionSteps = 1000000;
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(nullptr, config));
        for (const auto &[function, op] :
             std::vector<std::pair<const char *, Operation>>{{"cell", Operation::Cell},
                                                             {"emit", Operation::Emit},
                                                             {"range_count", Operation::RangeCount},
                                                             {"range_value", Operation::RangeValue},
                                                             {"range_numeric", Operation::RangeNumeric}})
        {
            auto binding = std::make_unique<Binding>(*this, op);
            vm->addFunction(function, binding.get());
            binding.release();
        }
        if (!vm->load(bytes))
            throw std::runtime_error("Could not load formula bytecode");
        ++compilations;
        return vm;
    }

    bool invoke(Operation operation, kern::IExternCall &call)
    {
        if (!m_active || !m_data)
            throw kern::RuntimeError("No formula is active");
        if (operation == Operation::Emit)
        {
            if (!call.getArgument(0, m_emitted))
                return false;
            ++m_emitCount;
            return true;
        }
        int first = -1;
        if (!call.getArgument(0, first))
            return false;
        int cell = first;
        if (operation != Operation::Cell)
        {
            if (first < 0 || first >= static_cast<int>(m_active->ranges.size()))
                throw kern::RuntimeError("Invalid formula range");
            const auto &range = m_active->ranges[first];
            if (operation == Operation::RangeCount)
            {
                call.setReturnValue(static_cast<int>(range.size()));
                return true;
            }
            int offset = -1;
            if (!call.getArgument(1, offset))
                return false;
            if (offset < 0 || offset >= static_cast<int>(range.size()))
                throw kern::RuntimeError("Invalid range offset");
            cell = range[offset];
        }
        if (cell < 0 || cell >= static_cast<int>(m_results.size()))
            throw kern::RuntimeError("Invalid cell reference");
        const auto &result = m_results[cell];
        if (!result.error.empty())
            throw kern::RuntimeError("Referenced cell has an error");
        if (operation == Operation::RangeNumeric)
            call.setReturnValue(result.number);
        else
            call.setReturnValue(result.number ? result.value : 0.0);
        return true;
    }

    void classifyCycles(const std::vector<int> &pending, const std::vector<std::vector<int>> &dependents)
    {
        // Iterative Kosaraju distinguishes actual cycles from their downstream
        // dependents without placing a large workbook graph on the C++ stack.
        std::vector<bool> seen(m_plans.size(), false);
        std::vector<int> finish;
        for (int root = 0; root < static_cast<int>(m_plans.size()); ++root)
        {
            if (!pending[root] || seen[root])
                continue;
            std::vector<std::pair<int, std::size_t>> stack{{root, 0}};
            seen[root] = true;
            while (!stack.empty())
            {
                auto &[node, cursor] = stack.back();
                const auto &deps = m_plans[node].dependencies;
                if (cursor == deps.size())
                {
                    finish.push_back(node);
                    stack.pop_back();
                    continue;
                }
                const int next = deps[cursor++];
                if (pending[next] && !seen[next])
                {
                    seen[next] = true;
                    stack.push_back({next, 0});
                }
            }
        }
        std::fill(seen.begin(), seen.end(), false);
        for (auto it = finish.rbegin(); it != finish.rend(); ++it)
        {
            if (seen[*it])
                continue;
            std::vector<int> component, stack{*it};
            seen[*it] = true;
            while (!stack.empty())
            {
                const int node = stack.back();
                stack.pop_back();
                component.push_back(node);
                for (int next : dependents[node])
                    if (pending[next] && !seen[next])
                    {
                        seen[next] = true;
                        stack.push_back(next);
                    }
            }
            const auto &deps = m_plans[component[0]].dependencies;
            const bool cycle = component.size() > 1 || std::find(deps.begin(), deps.end(), component[0]) != deps.end();
            for (int node : component)
                m_results[node] = {false, 0, cycle ? "#CYCLE!" : "#DEPEND!",
                                   cycle ? "Circular cell reference" : "Depends on a circular cell reference"};
        }
    }

public:
    std::size_t compilations = 0, calls = 0, recalculated = 0;
    explicit Evaluator(std::vector<std::string> searchPaths) : m_paths(std::move(searchPaths)) {}
    Evaluator(const Evaluator &) = delete;
    Evaluator &operator=(const Evaluator &) = delete;
    const std::vector<Result> &results() const { return m_results; }
    bool current() const { return m_current; }
    const std::vector<int> &dependencies(int index) const
    {
        static const std::vector<int> empty;
        return m_current ? m_plans.at(index).dependencies : empty;
    }

    void recalculate(const Data &data, bool force = false)
    {
        const bool wasCurrent = m_current;
        m_current = false;
        data.validate();
        recalculated = 0;
        const bool resized = m_rows != data.rows || m_columns != data.columns;
        std::vector<bool> changed(data.cells.size(), resized || m_force || force || !wasCurrent);
        std::vector<std::optional<Plan>> replacements(data.cells.size());
        std::size_t edges = 0;
        for (int i = 0; i < static_cast<int>(data.cells.size()); ++i)
        {
            if (resized || m_force || m_plans[i].text != data.cells[i])
            {
                changed[i] = true;
                Plan plan;
                plan.text = data.cells[i];
                if (!plan.text.empty() && plan.text[0] == '=')
                {
                    try
                    {
                        plan = parse(data, plan.text);
                    }
                    catch (const std::exception &error)
                    {
                        plan.error = error.what();
                    }
                }
                replacements[i] = std::move(plan);
            }
            const auto &candidate = replacements[i] ? *replacements[i] : m_plans[i];
            edges += candidate.dependencies.size();
            if (edges > 1000000)
                throw std::runtime_error("Workbook exceeds one million formula references");
        }
        // Commit plan changes only after validating the complete candidate
        // graph. An aborted edit cannot make a stale result look up to date.
        if (resized)
        {
            std::vector<Plan> plans(data.cells.size());
            std::vector<Result> results(data.cells.size());
            m_plans.swap(plans);
            m_results.swap(results);
            m_rows = data.rows;
            m_columns = data.columns;
        }
        for (std::size_t i = 0; i < replacements.size(); ++i)
            if (replacements[i])
                m_plans[i] = std::move(*replacements[i]);
        std::vector<std::vector<int>> dependents(data.cells.size());
        std::vector<int> pending(data.cells.size(), 0);
        for (int i = 0; i < static_cast<int>(m_plans.size()); ++i)
            for (int dep : m_plans[i].dependencies)
            {
                dependents[dep].push_back(i);
                ++pending[i];
            }
        std::vector<int> dirty;
        for (int i = 0; i < static_cast<int>(changed.size()); ++i)
            if (changed[i])
                dirty.push_back(i);
        for (std::size_t head = 0; head < dirty.size(); ++head)
            for (int next : dependents[dirty[head]])
                if (!changed[next])
                {
                    changed[next] = true;
                    dirty.push_back(next);
                }
        std::vector<int> order;
        for (int i = 0; i < static_cast<int>(pending.size()); ++i)
            if (!pending[i])
                order.push_back(i);
        for (std::size_t head = 0; head < order.size(); ++head)
            for (int next : dependents[order[head]])
                if (--pending[next] == 0)
                    order.push_back(next);
        m_data = &data;
        struct ContextReset
        {
            const Data *&data;
            Plan *&active;
            ~ContextReset()
            {
                data = nullptr;
                active = nullptr;
            }
        } contextReset{m_data, m_active};
        for (int index : order)
        {
            if (!changed[index])
                continue;
            auto &result = m_results[index];
            auto &plan = m_plans[index];
            result = {};
            if (plan.text.empty() || plan.text[0] != '=')
            {
                result.display = plan.text;
                float value = 0;
                const auto parsed = std::from_chars(plan.text.data(), plan.text.data() + plan.text.size(), value,
                                                    std::chars_format::general);
                if (!plan.text.empty() && parsed.ec == std::errc{} &&
                    parsed.ptr == plan.text.data() + plan.text.size() && std::isfinite(value))
                {
                    result.number = true;
                    result.value = value;
                }
                continue;
            }
            ++recalculated;
            if (!plan.error.empty())
            {
                result = {false, 0, "#FORMULA!", plan.error};
                continue;
            }
            bool badDependency = false;
            for (int dep : plan.dependencies)
                if (!m_results[dep].error.empty())
                    badDependency = true;
            if (badDependency)
            {
                result = {false, 0, "#DEPEND!", "A referenced cell contains an error"};
                continue;
            }
            try
            {
                if (!plan.vm)
                    plan.vm = compile(plan, index);
                m_active = &plan;
                m_emitCount = 0;
                const bool executed = plan.vm->callFunction("calc");
                ++calls;
                m_active = nullptr;
                if (!executed || m_emitCount != 1 || !std::isfinite(m_emitted))
                    throw std::runtime_error("Formula must produce one finite numeric value");
                result = {true, m_emitted, format(m_emitted), {}};
            }
            catch (const std::exception &error)
            {
                m_active = nullptr;
                result = {false, 0, plan.vm ? "#VALUE!" : "#FORMULA!", error.what()};
            }
        }
        classifyCycles(pending, dependents);
        m_force = false;
        m_current = true;
    }

    void reloadHelpers(const Data &data)
    {
        // Compile replacements first. If any working formula fails to compile,
        // leave every existing VM and result available to the caller.
        std::vector<std::pair<int, std::unique_ptr<kern::IVirtualMachine>>> candidates;
        for (int i = 0; i < static_cast<int>(m_plans.size()); ++i)
            if (m_plans[i].vm)
                candidates.emplace_back(i, compile(m_plans[i], i));
        if (candidates.empty())
        {
            Plan probe;
            probe.expression = "1.0";
            auto vm = compile(probe, 0);
        }
        for (auto &[index, vm] : candidates)
            m_plans[index].vm = std::move(vm);
        recalculate(data, true);
    }
};
} // namespace gridbook
