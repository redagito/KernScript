#include "vm/VirtualMachine.h"
#include "kern/RuntimeError.h"
#include "logging/Logger.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <iomanip>
#include <limits>
#include <ostream>
#include <span>

namespace
{
class ExternCall final : public kern::IExternCall
{
    std::span<const Value> m_arguments;
    kern::ReturnKind m_kind;
    std::shared_ptr<Value::StringBudget> m_budget;
    Value m_result;
    bool m_returned = false;
    template <class T> bool read(unsigned int index, T &out) const
    {
        if (index >= m_arguments.size())
            throw kern::RuntimeError("Host argument index out of range.");
        return m_arguments[index].convert(out);
    }
    void requireResult() const
    {
        if (m_kind != kern::ReturnKind::Value || m_returned)
            throw kern::RuntimeError("Host result does not match its signature.");
    }
    void result(Value value)
    {
        if (m_kind != kern::ReturnKind::Value || m_returned)
            throw kern::RuntimeError("Host result does not match its signature.");
        m_result = std::move(value);
        m_returned = true;
    }

public:
    ExternCall(std::span<const Value> arguments, kern::ReturnKind kind, std::shared_ptr<Value::StringBudget> budget)
        : m_arguments(arguments), m_kind(kind), m_budget(std::move(budget))
    {
    }
    unsigned int getArgumentCount() const override { return static_cast<unsigned int>(m_arguments.size()); }
    bool getArgument(unsigned int i, int &v) const override { return read(i, v); }
    bool getArgument(unsigned int i, unsigned int &v) const override { return read(i, v); }
    bool getArgument(unsigned int i, float &v) const override { return read(i, v); }
    bool getArgument(unsigned int i, double &v) const override { return read(i, v); }
    bool getArgument(unsigned int i, bool &v) const override { return read(i, v); }
    bool getArgument(unsigned int i, std::string &v) const override { return read(i, v); }
    void setReturnValue(int v) override { result(Value(v)); }
    void setReturnValue(unsigned int v) override { result(Value(static_cast<uint32_t>(v))); }
    void setReturnValue(float v) override { result(Value(v)); }
    void setReturnValue(double v) override { result(Value(static_cast<float>(v))); }
    void setReturnValue(bool v) override { result(Value(static_cast<int32_t>(v))); }
    void setReturnValue(const std::string &v) override
    {
        requireResult();
        result(Value(v, m_budget));
    }
    Value finish()
    {
        if ((m_kind == kern::ReturnKind::Value) != m_returned)
            throw kern::RuntimeError("Host function did not produce its declared result.");
        return std::move(m_result);
    }
};
} // namespace
VirtualMachine::VirtualMachine(kern::ILogger *logger) : VirtualMachine(logger, kern::VMConfig{}) {}
VirtualMachine::VirtualMachine(kern::ILogger *logger, kern::VMConfig config)
    : m_logger(logger ? logger : nullLogger()), m_config(config),
      m_stringBudget(std::make_shared<Value::StringBudget>(Value::StringBudget{config.maxRuntimeStringBytes}))
{
    if (config.maxStackBytes > std::vector<Value>().max_size() * sizeof(Value))
        fail("Maximum runtime stack size exceeds the limit.");
}
[[noreturn]] void VirtualMachine::fail(const std::string &message) const
{
    m_logger->error("{}", message);
    throw kern::RuntimeError(message);
}
void VirtualMachine::idle(const char *what) const
{
    if (m_running)
        fail(std::string(what) + " is not reentrant.");
}
void VirtualMachine::clearScripts()
{
    idle("clearScripts");
    m_frames.clear();
    m_locals.clear();
    m_operands.clear();
    std::vector<Value>().swap(m_constants);
    m_program.reset();
}
bool VirtualMachine::load(std::istream &input)
{
    idle("load");
    clearScripts();
    try
    {
        Program decoded;
        std::string error;
        if (!deserializeProgram(input, decoded, m_config, error))
        {
            if (!error.empty())
                m_logger->error("{}", error);
            return false;
        }
        // The cached immutable Values share all runtime copies. Count the pool
        // cache and its contents as well as the decoded program before allocating.
        size_t remaining = m_config.maxProgramBytes;
        auto charge = [&](size_t count, size_t width = 1)
        {
            if (width && count > remaining / width)
                fail("Maximum loaded program bytes exceeded.");
            remaining -= count * width;
        };
        charge(decoded.moduleName.size());
        charge(decoded.strings.size(), sizeof(std::string) + sizeof(Value) + Value::stringStorageBytes());
        charge(decoded.externs.size(), sizeof(ProgramExtern));
        charge(decoded.functions.size(), sizeof(ProgramFunction));
        for (const auto &s : decoded.strings)
        {
            charge(s.size(), 2);
        }
        for (const auto &e : decoded.externs)
            charge(e.name.size());
        for (const auto &f : decoded.functions)
        {
            charge(f.name.size());
            charge(f.instructions.size(), sizeof(Instruction));
        }
        auto verified = verifyProgram(std::move(decoded), error);
        if (!verified)
        {
            m_logger->error("{}", error);
            return false;
        }
        std::vector<Value> constants;
        constants.reserve(verified->data().strings.size());
        for (const auto &s : verified->data().strings)
            constants.emplace_back(s);
        m_constants = std::move(constants);
        m_program = std::move(verified);
        return true;
    }
    catch (const std::bad_alloc &)
    {
        clearScripts();
        fail("Program allocation failed.");
    }
    catch (const std::length_error &)
    {
        clearScripts();
        fail("Program allocation exceeds the limit.");
    }
    catch (...)
    {
        clearScripts();
        throw;
    }
}
void VirtualMachine::addFunction(const std::string &name, kern::IExternFunction *fn)
{
    idle("addFunction");
    if (name.empty() || !fn)
        fail("Extern name and binding must not be empty.");
    const auto sig = fn->getSignature();
    if (sig.returnKind != kern::ReturnKind::Void && sig.returnKind != kern::ReturnKind::Value)
        fail("Invalid host signature.");
    m_bindings[name].reset(fn);
}
void VirtualMachine::removeFunction(const std::string &name)
{
    idle("removeFunction");
    m_bindings.erase(name);
}
void VirtualMachine::checkStack(size_t locals, size_t operands) const
{
    const size_t max = m_config.maxStackBytes / sizeof(Value);
    if (locals > max || operands > max - locals)
        fail("Maximum runtime stack size exceeded.");
}
void VirtualMachine::push(Value value)
{
    if (value.getType() == Value::Type::Invalid)
        fail("Push of an uninitialized value.");
    checkStack(m_locals.size(), m_operands.size() + 1);
    m_operands.push_back(std::move(value));
}
Value VirtualMachine::pop()
{
    if (m_frames.empty() || m_operands.size() <= m_frames.back().operandBase)
        fail("Operand stack underflow at frame boundary.");
    Value result = std::move(m_operands.back());
    m_operands.pop_back();
    return result;
}
Value &VirtualMachine::local(int32_t raw)
{
    const auto &frame = m_frames.back();
    const auto &f = m_program->data().functions[frame.function];
    if (raw < 0 || static_cast<uint32_t>(raw) >= f.frameSize)
        fail("Local index outside its frame.");
    return m_locals.at(frame.localsBase + static_cast<uint32_t>(raw));
}
void VirtualMachine::enter(uint32_t target)
{
    const auto &f = m_program->data().functions.at(target);
    const size_t count = f.signature.argumentCount;
    const size_t floor = m_frames.empty() ? 0 : m_frames.back().operandBase;
    if (m_operands.size() < floor || count > m_operands.size() - floor)
        fail("Call arguments cross the caller's operand boundary.");
    if (!m_frames.empty() && m_frames.size() - 1 >= m_config.maxCallDepth)
        fail("Maximum call depth exceeded.");
    const size_t base = m_locals.size(), argumentBase = m_operands.size() - count;
    if (f.frameSize > std::numeric_limits<size_t>::max() - base)
        fail("Function frame exceeds the limit.");
    checkStack(base + f.frameSize, argumentBase);
    m_locals.resize(base + f.frameSize);
    for (size_t i = 0; i < count; ++i)
        m_locals[base + i] = std::move(m_operands[argumentBase + i]);
    m_operands.resize(argumentBase);
    m_frames.push_back({target, 0, base, argumentBase});
}
bool VirtualMachine::callFunction(const std::string &name)
{
    idle("callFunction");
    if (!m_program)
        return false;
    const auto &p = m_program->data();
    auto it = std::find_if(p.functions.begin(), p.functions.end(), [&](const auto &f) { return f.name == name; });
    if (it == p.functions.end())
        return false;
    if (it->signature != kern::FunctionSignature{0, kern::ReturnKind::Void})
        fail("Entry function must have zero arguments and return void.");
    m_frames.clear();
    m_locals.clear();
    m_operands.clear();
    m_running = true;
    struct Guard
    {
        VirtualMachine &vm;
        ~Guard()
        {
            vm.m_frames.clear();
            vm.m_locals.clear();
            vm.m_operands.clear();
            vm.m_running = false;
        }
    } guard{*this};
    try
    {
        if (m_profile)
            ++m_profile->calls;
        enter(static_cast<uint32_t>(it - p.functions.begin()));
        uint64_t steps = 0;
        while (true)
        {
            if (steps >= m_config.maxExecutionSteps)
                fail("Maximum execution steps exceeded.");
            ++steps;
            if (!step())
                break;
        }
        return true;
    }
    catch (const std::bad_alloc &)
    {
        fail("Runtime allocation failed.");
    }
    catch (const std::length_error &)
    {
        fail("Runtime allocation exceeds the limit.");
    }
}
bool VirtualMachine::step()
{
    const auto fetchStart = m_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto &frame = m_frames.back();
    const auto &p = m_program->data();
    const auto &function = p.functions[frame.function];
    if (frame.ip >= function.instructions.size())
        fail("Execution fell through a verified function.");
    const auto in = function.instructions[frame.ip++];
    const auto raw = static_cast<size_t>(in.id);
    const auto started = m_profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    struct Probe
    {
        Profile *profile;
        size_t raw;
        std::chrono::steady_clock::time_point start;
        ~Probe()
        {
            if (profile)
                profile->ns[raw] += static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start)
                        .count());
        }
    } probe{m_profile.get(), raw, started};
    if (m_profile)
    {
        ++m_profile->steps;
        ++m_profile->count[raw];
        m_profile->fetchNs +=
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(started - fetchStart).count());
    }
    auto numeric = [&](const Value &v)
    {
        if (!v.isNumeric())
            fail("Instruction needs numeric operands.");
    };
    switch (in.id)
    {
    case Opcode::Nop:
    case Opcode::Break:
        break;
    case Opcode::Exit:
        return false;
    case Opcode::Movv:
        if (local(in.args[1]).getType() == Value::Type::Invalid)
            fail("Read of an uninitialized local.");
        local(in.args[0]) = local(in.args[1]);
        break;
    case Opcode::Movi:
        local(in.args[0]) = Value(in.args[1]);
        break;
    case Opcode::Movf:
        local(in.args[0]) = Value(std::bit_cast<float>(in.args[1]));
        break;
    case Opcode::Movs:
        local(in.args[0]) = m_constants.at(in.args[1]);
        break;
    case Opcode::Pushv:
        push(local(in.args[0]));
        break;
    case Opcode::Pushi:
        push(Value(in.args[0]));
        break;
    case Opcode::Pushu:
        push(Value(std::bit_cast<uint32_t>(in.args[0])));
        break;
    case Opcode::Pushf:
        push(Value(std::bit_cast<float>(in.args[0])));
        break;
    case Opcode::Pushs:
        push(m_constants.at(in.args[0]));
        break;
    case Opcode::Pop:
        pop();
        break;
    case Opcode::Popv:
    {
        auto value = pop();
        local(in.args[0]) = std::move(value);
        break;
    }
    case Opcode::Call:
        enter(static_cast<uint32_t>(in.args[0]));
        break;
    case Opcode::Calle:
    {
        const auto &ext = p.externs.at(in.args[0]);
        auto binding = m_bindings.find(ext.name);
        if (binding == m_bindings.end())
            fail("The extern function '" + ext.name + "' does not exist.");
        if (binding->second->getSignature() != ext.signature)
            fail("Host signature mismatch for '" + ext.name + "'.");
        const size_t count = ext.signature.argumentCount;
        if (m_operands.size() < frame.operandBase || count > m_operands.size() - frame.operandBase)
            fail("Host arguments cross the operand boundary.");
        const auto first = m_operands.size() - count;
        ExternCall call(std::span<const Value>(m_operands).subspan(first, count), ext.signature.returnKind,
                        m_stringBudget);
        if (!binding->second->call(call))
            fail("The extern function '" + ext.name + "' reported failure.");
        auto result = call.finish();
        m_operands.resize(first);
        if (ext.signature.returnKind == kern::ReturnKind::Value)
            push(std::move(result));
        break;
    }
    case Opcode::Ret:
    {
        const bool valued = function.signature.returnKind == kern::ReturnKind::Value;
        if (m_operands.size() - frame.operandBase != static_cast<size_t>(valued))
            fail("Return stack does not match signature.");
        Value result;
        if (valued)
            result = pop();
        const size_t base = frame.localsBase, floor = frame.operandBase;
        m_locals.resize(base);
        m_operands.resize(floor);
        m_frames.pop_back();
        if (m_frames.empty())
            return false;
        if (valued)
            push(std::move(result));
        break;
    }
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div:
    {
        auto y = pop(), x = pop();
        numeric(x);
        numeric(y);
        switch (in.id)
        {
        case Opcode::Add:
            x += y;
            break;
        case Opcode::Sub:
            x -= y;
            break;
        case Opcode::Mul:
            x *= y;
            break;
        case Opcode::Div:
            if (y.isZero())
                fail("Division by zero.");
            x /= y;
            break;
        default:
            break;
        }
        push(std::move(x));
        break;
    }
    case Opcode::Inc:
    case Opcode::Dec:
    {
        auto x = pop();
        numeric(x);
        if (in.id == Opcode::Inc)
            x += Value(1);
        else
            x -= Value(1);
        push(std::move(x));
        break;
    }
    case Opcode::Neg:
        push(pop().negated());
        break;
    case Opcode::Not:
        push(Value(static_cast<int32_t>(!pop().truthy())));
        break;
    case Opcode::And:
    case Opcode::Or:
    case Opcode::Xor:
    {
        const bool y = pop().truthy(), x = pop().truthy();
        const bool result = in.id == Opcode::And ? x && y : in.id == Opcode::Or ? x || y : x != y;
        push(Value(static_cast<int32_t>(result)));
        break;
    }
    case Opcode::Jmp:
        frame.ip = static_cast<uint32_t>(in.args[0]);
        break;
    case Opcode::Je:
    case Opcode::Jne:
    case Opcode::Jg:
    case Opcode::Jge:
    case Opcode::Jl:
    case Opcode::Jle:
    {
        auto y = pop(), x = pop();
        bool take = false;
        switch (in.id)
        {
        case Opcode::Je:
            take = x == y;
            break;
        case Opcode::Jne:
            take = x != y;
            break;
        case Opcode::Jg:
            take = x > y;
            break;
        case Opcode::Jge:
            take = x >= y;
            break;
        case Opcode::Jl:
            take = x < y;
            break;
        default:
            take = x <= y;
            break;
        }
        if (take)
            frame.ip = static_cast<uint32_t>(in.args[0]);
        break;
    }
    default:
        fail("Noncanonical opcode reached execution.");
    }
    return true;
}
void VirtualMachine::resetProfile()
{
    idle("resetProfile");
    m_profile = std::make_unique<Profile>();
}
void VirtualMachine::dumpProfile(std::ostream &out)
{
    static const Profile empty;
    const auto &p = m_profile ? *m_profile : empty;
    uint64_t total = p.fetchNs;
    for (auto n : p.ns)
        total += n;
    out << "profile: " << p.steps << " iterations, " << p.calls << " calls, " << total
        << " ns total\nmnemonic count cycles_ns share_pct\n";
    struct Row
    {
        std::string name;
        uint64_t count;
        uint64_t ns;
    };
    std::vector<Row> rows;
    for (size_t i = 0; i < p.count.size(); ++i)
        if (p.count[i])
            rows.push_back({instructions[i].mnemonic, p.count[i], p.ns[i]});
    if (p.steps)
        rows.push_back({"fetch", p.steps, p.fetchNs});
    std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.ns > b.ns; });
    for (const auto &row : rows)
        out << row.name << ' ' << row.count << ' ' << row.ns << ' ' << std::fixed << std::setprecision(1)
            << (total ? 100.0 * static_cast<double>(row.ns) / static_cast<double>(total) : 0) << "%\n";
}
