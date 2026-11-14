#include "common/Program.h"
#include "kern/RuntimeError.h"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstring>
#include <deque>
#include <iomanip>
#include <istream>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace
{
bool validKind(kern::ReturnKind kind) { return kind == kern::ReturnKind::Void || kind == kern::ReturnKind::Value; }
std::string where(const ProgramFunction &f, size_t pc)
{
    if (pc < f.locations.size() && !f.locations[pc].source.empty())
    {
        const auto &p = f.locations[pc];
        return p.source + ":" + std::to_string(p.line) + ":" + std::to_string(p.column) + ": ";
    }
    return "Function '" + f.name + "', instruction " + std::to_string(pc) + ": ";
}
bool pseudo(Opcode op) { return op == Opcode::Retv || op == Opcode::Reti || op == Opcode::Retf || op == Opcode::Rets; }
bool jump(Opcode op) { return op >= Opcode::Jmp && op <= Opcode::Jle; }
void put(std::ostream &s, uint64_t v, unsigned bytes)
{
    for (unsigned i = 0; i < bytes; ++i)
        s.put(static_cast<char>((v >> (i * 8)) & 255));
}
bool get(std::istream &s, uint64_t &v, unsigned bytes)
{
    v = 0;
    for (unsigned i = 0; i < bytes; ++i)
    {
        int c = s.get();
        if (c == std::char_traits<char>::eof())
            return false;
        v |= uint64_t(static_cast<unsigned char>(c)) << (i * 8);
    }
    return true;
}
void putString(std::ostream &s, const std::string &v)
{
    put(s, v.size(), 4);
    s.write(v.data(), static_cast<std::streamsize>(v.size()));
}
struct Reader
{
    std::istream &s;
    kern::VMConfig config;
    size_t remaining;
    void charge(uint64_t count, size_t width = 1)
    {
        if (width && count > remaining / width)
            throw kern::RuntimeError("Maximum loaded program bytes exceeded.");
        remaining -= static_cast<size_t>(count) * width;
    }
    bool number(uint64_t &v, unsigned bytes = 4) { return get(s, v, bytes); }
    bool count(uint64_t &v, size_t width)
    {
        if (!number(v))
            return false;
        if (v > config.maxTableEntries)
            throw kern::RuntimeError("Table entry limit exceeded.");
        charge(v, width);
        return true;
    }
    bool string(std::string &v)
    {
        uint64_t n = 0;
        if (!number(n))
            return false;
        if (n > config.maxStringBytes)
            throw kern::RuntimeError("String length exceeds the limit.");
        charge(n);
        v.resize(static_cast<size_t>(n));
        if (n)
            s.read(v.data(), static_cast<std::streamsize>(n));
        return bool(s);
    }
    bool signature(kern::FunctionSignature &sig)
    {
        uint64_t n = 0, k = 0;
        if (!number(n) || !number(k, 1))
            return false;
        sig = {static_cast<uint32_t>(n), static_cast<kern::ReturnKind>(k)};
        return true;
    }
};
} // namespace

bool linkProgram(const ir::Program &input, Program &out, std::string &error)
{
    out = {};
    error.clear();
    out.moduleName = input.moduleName;
    std::map<std::string, uint32_t> funcs, exts, strings;
    try
    {
        for (const auto &ext : input.externs)
        {
            if (ext.name.empty() || !exts.emplace(ext.name, static_cast<uint32_t>(out.externs.size())).second)
                throw std::runtime_error("Duplicate or empty extern name.");
            out.externs.push_back({ext.name, ext.signature});
        }
        for (const auto &f : input.functions)
        {
            if (f.name.empty() || exts.contains(f.name) ||
                !funcs.emplace(f.name, static_cast<uint32_t>(funcs.size())).second)
                throw std::runtime_error("Duplicate or empty function name.");
        }
        auto constant = [&](const std::string &value) -> int32_t
        {
            auto [it, added] = strings.emplace(value, static_cast<uint32_t>(out.strings.size()));
            if (added)
                out.strings.push_back(value);
            if (it->second > INT32_MAX)
                throw std::runtime_error("Too many constants.");
            return static_cast<int32_t>(it->second);
        };
        for (const auto &f : input.functions)
        {
            ProgramFunction result;
            result.name = f.name;
            result.signature = f.signature;
            if (f.params.size() != f.signature.argumentCount)
                throw std::runtime_error("Signature does not match parameter declarations in '" + f.name + "'.");
            std::map<std::string, uint32_t> locals, labels;
            uint64_t slots = 0, pc = 0;
            for (const auto &p : f.params)
            {
                if (!locals.emplace(p, static_cast<uint32_t>(slots++)).second)
                    throw std::runtime_error("Duplicate parameter.");
            }
            for (const auto &v : f.locals)
            {
                if (v.count == 0 || slots + v.count > INT32_MAX ||
                    !locals.emplace(v.name, static_cast<uint32_t>(slots)).second)
                    throw std::runtime_error("Invalid or duplicate local declaration.");
                slots += v.count;
            }
            result.frameSize = static_cast<uint32_t>(slots);
            for (const auto &item : f.code.items)
            {
                if (const auto *label = std::get_if<ir::Label>(&item))
                {
                    if (!labels.emplace(label->name, static_cast<uint32_t>(pc)).second)
                        throw std::runtime_error("Duplicate label.");
                }
                else
                    pc += pseudo(std::get<ir::Operation>(item).id) ? 2 : 1;
                if (pc > INT32_MAX)
                    throw std::runtime_error("Too many instructions.");
            }
            auto index = [](const auto &map, const std::string &name, const char *kind) -> int32_t
            {
                auto found = map.find(name);
                if (found == map.end())
                    throw std::runtime_error(std::string("Unknown ") + kind + " '" + name + "'.");
                return static_cast<int32_t>(found->second);
            };
            for (const auto &item : f.code.items)
            {
                const auto *op = std::get_if<ir::Operation>(&item);
                if (!op)
                    continue;
                auto raw = static_cast<size_t>(op->id);
                if (raw >= std::size(instructions) || op->operands.size() != instructions[raw].argCount)
                    throw std::runtime_error("Invalid opcode or operand count.");
                Instruction instruction;
                instruction.id = op->id;
                for (size_t i = 0; i < op->operands.size(); ++i)
                {
                    const auto &operand = op->operands[i];
                    const auto expected = instructions[raw].operands[i];
                    switch (expected)
                    {
                    case OperandKind::Local:
                        instruction.args[i] = index(locals, std::get<ir::Local>(operand).name, "local");
                        break;
                    case OperandKind::String:
                        instruction.args[i] = constant(std::get<ir::String>(operand).value);
                        break;
                    case OperandKind::Function:
                        instruction.args[i] = index(funcs, std::get<ir::FunctionRef>(operand).name, "function");
                        break;
                    case OperandKind::Extern:
                        instruction.args[i] = index(exts, std::get<ir::ExternRef>(operand).name, "extern");
                        break;
                    case OperandKind::Label:
                        instruction.args[i] = index(labels, std::get<ir::LabelRef>(operand).name, "label");
                        break;
                    case OperandKind::Integer:
                        instruction.args[i] = std::get<int32_t>(operand);
                        break;
                    case OperandKind::Unsigned:
                        instruction.args[i] = std::bit_cast<int32_t>(std::get<uint32_t>(operand));
                        break;
                    case OperandKind::Float:
                        instruction.args[i] = std::bit_cast<int32_t>(std::get<float>(operand));
                        break;
                    default:
                        throw std::runtime_error("Unexpected operand.");
                    }
                }
                if (pseudo(instruction.id))
                {
                    switch (instruction.id)
                    {
                    case Opcode::Retv:
                        instruction.id = Opcode::Pushv;
                        break;
                    case Opcode::Reti:
                        instruction.id = Opcode::Pushi;
                        break;
                    case Opcode::Retf:
                        instruction.id = Opcode::Pushf;
                        break;
                    case Opcode::Rets:
                        instruction.id = Opcode::Pushs;
                        break;
                    default:
                        break;
                    }
                    result.instructions.push_back(instruction);
                    result.locations.push_back(op->location);
                    instruction = {};
                    instruction.id = Opcode::Ret;
                }
                result.instructions.push_back(instruction);
                result.locations.push_back(op->location);
            }
            // A trailing label in a void function points to this explicit return.
            if (f.signature.returnKind == kern::ReturnKind::Void)
            {
                Instruction ret;
                ret.id = Opcode::Ret;
                result.instructions.push_back(ret);
                result.locations.push_back(f.location);
            }
            out.functions.push_back(std::move(result));
        }
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        out = {};
        return false;
    }
}

std::optional<VerifiedProgram> verifyProgram(Program p, std::string &error)
{
    error.clear();
    auto fail = [&](const std::string &message) -> std::optional<VerifiedProgram>
    {
        error = message;
        return std::nullopt;
    };
    std::set<std::string> names;
    for (const auto &e : p.externs)
        if (e.name.empty() || !names.insert(e.name).second || !validKind(e.signature.returnKind))
            return fail("Invalid extern signature or duplicate name.");
    for (const auto &f : p.functions)
        if (f.name.empty() || !names.insert(f.name).second || !validKind(f.signature.returnKind) ||
            f.frameSize < f.signature.argumentCount)
            return fail("Invalid function signature or frame layout.");
    for (const auto &f : p.functions)
    {
        const size_t n = f.instructions.size();
        if (n == 0)
            return fail("Function '" + f.name + "' has reachable fallthrough.");
        auto range = [](int32_t i, size_t size) { return i >= 0 && static_cast<uint64_t>(i) < size; };
        for (size_t pc = 0; pc < n; ++pc)
        {
            const auto &in = f.instructions[pc];
            const auto op = in.id;
            bool okay = static_cast<size_t>(op) < std::size(instructions) && !pseudo(op);
            if (okay)
                for (unsigned i = 0; i < instructions[static_cast<size_t>(op)].argCount; ++i)
                {
                    switch (instructions[static_cast<size_t>(op)].operands[i])
                    {
                    case OperandKind::Local:
                        okay = okay && range(in.args[i], f.frameSize);
                        break;
                    case OperandKind::String:
                        okay = okay && range(in.args[i], p.strings.size());
                        break;
                    case OperandKind::Function:
                        okay = okay && range(in.args[i], p.functions.size());
                        break;
                    case OperandKind::Extern:
                        okay = okay && range(in.args[i], p.externs.size());
                        break;
                    case OperandKind::Label:
                        okay = okay && in.args[i] >= 0 && static_cast<uint64_t>(in.args[i]) <= n;
                        break;
                    default:
                        break;
                    }
                }
            if (!okay)
                return fail(where(f, pc) + "Invalid opcode, operand reference, or jump target.");
        }
        std::vector<int64_t> height(n, -1);
        std::deque<size_t> work;
        height[0] = 0;
        work.push_back(0);
        while (!work.empty())
        {
            const auto pc = work.front();
            work.pop_front();
            const auto &in = f.instructions[pc];
            const auto op = in.id;
            const auto &schema = instructions[static_cast<size_t>(op)];
            int64_t pop = schema.pops, push = schema.pushes;
            if (schema.flow == FlowKind::Call)
            {
                const auto sig = p.functions[in.args[0]].signature;
                pop = sig.argumentCount;
                push = kern::returnCount(sig);
            }
            else if (schema.flow == FlowKind::ExternCall)
            {
                const auto sig = p.externs[in.args[0]].signature;
                pop = sig.argumentCount;
                push = kern::returnCount(sig);
            }
            else if (schema.flow == FlowKind::Return)
            {
                if (height[pc] != kern::returnCount(f.signature))
                    return fail(where(f, pc) + "Return stack does not match function signature.");
                continue;
            }
            else if (schema.flow == FlowKind::Exit)
                continue;
            if (height[pc] < pop)
                return fail(where(f, pc) + "Operand stack underflow.");
            const int64_t after = height[pc] - pop + push;
            auto edge = [&](size_t target) -> bool
            {
                if (target >= n)
                {
                    error = where(f, pc) + "Reachable fallthrough from function.";
                    return false;
                }
                if (height[target] != -1 && height[target] != after)
                {
                    error = where(f, target) + "Inconsistent operand stack height at control-flow join.";
                    return false;
                }
                if (height[target] == -1)
                {
                    height[target] = after;
                    work.push_back(target);
                }
                return true;
            };
            if (jump(op) && !edge(static_cast<size_t>(in.args[0])))
                return std::nullopt;
            if (op != Opcode::Jmp && !edge(pc + 1))
                return std::nullopt;
        }
    }
    return VerifiedProgram(std::move(p));
}

void emitAssembly(const ir::Program &p, std::ostream &out)
{
    auto kind = [](kern::ReturnKind k) { return k == kern::ReturnKind::Value ? "value" : "void"; };
    // Names are quoted and decoded contextually, so assembly keywords do not leak into source identifiers.
    auto name = [&](const std::string &s) { out << std::quoted(s); };
    if (!p.moduleName.empty())
    {
        out << "module ";
        name(p.moduleName);
        out << '\n';
    }
    for (const auto &e : p.externs)
    {
        out << "extern ";
        name(e.name);
        out << ' ' << e.signature.argumentCount << " -> " << kind(e.signature.returnKind) << '\n';
    }
    for (const auto &f : p.functions)
    {
        out << "func ";
        name(f.name);
        out << " -> " << kind(f.signature.returnKind) << "\n{\n";
        for (const auto &v : f.params)
        {
            out << "param ";
            name(v);
            out << '\n';
        }
        for (const auto &v : f.locals)
        {
            out << (v.count == 1 ? "var " : "vara ");
            name(v.name);
            if (v.count != 1)
                out << ' ' << v.count;
            out << '\n';
        }
        for (const auto &item : f.code.items)
        {
            if (const auto *l = std::get_if<ir::Label>(&item))
            {
                out << "label ";
                name(l->name);
                out << '\n';
                continue;
            }
            const auto &op = std::get<ir::Operation>(item);
            out << instructions[static_cast<size_t>(op.id)].mnemonic;
            for (size_t i = 0; i < op.operands.size(); ++i)
            {
                out << (i == 0 ? " " : ", ");
                std::visit(
                    [&](const auto &v)
                    {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, ir::String>)
                            out << '"' << v.value << '"';
                        else if constexpr (std::is_same_v<T, float>)
                        {
                            char text[128];
                            const auto converted =
                                std::to_chars(std::begin(text), std::end(text), v, std::chars_format::fixed);
                            if (converted.ec != std::errc())
                                throw std::runtime_error("Cannot print float operand.");
                            out.write(text, converted.ptr - text);
                        }
                        else if constexpr (std::is_arithmetic_v<T>)
                            out << v;
                        else
                            name(v.name);
                    },
                    op.operands[i]);
            }
            out << '\n';
        }
        out << "}\n";
    }
}

bool serializeProgram(const Program &p, std::ostream &s)
{
    // Fixed-width, little-endian wire format, independent of host object layouts.
    s.write("KERN", 4);
    put(s, bytecodeMajorVersion, 1);
    put(s, bytecodeMinorVersion, 1);
    putString(s, p.moduleName);
    put(s, p.strings.size(), 4);
    for (const auto &v : p.strings)
        putString(s, v);
    put(s, p.externs.size(), 4);
    for (const auto &e : p.externs)
    {
        putString(s, e.name);
        put(s, e.signature.argumentCount, 4);
        put(s, static_cast<uint8_t>(e.signature.returnKind), 1);
    }
    put(s, p.functions.size(), 4);
    for (const auto &f : p.functions)
    {
        putString(s, f.name);
        put(s, f.signature.argumentCount, 4);
        put(s, static_cast<uint8_t>(f.signature.returnKind), 1);
        put(s, f.frameSize, 4);
        put(s, f.instructions.size(), 4);
        for (const auto &i : f.instructions)
        {
            const auto op = static_cast<size_t>(i.id);
            if (op >= std::size(instructions))
                return false;
            put(s, op, 2);
            for (size_t n = 0; n < instructions[op].argCount; ++n)
                put(s, std::bit_cast<uint32_t>(i.args[n]), 4);
        }
    }
    return bool(s);
}
bool deserializeProgram(std::istream &s, Program &p, kern::VMConfig config, std::string &error)
{
    p = {};
    Reader r{s, config, config.maxProgramBytes};
    try
    {
        char magic[4] = {};
        s.read(magic, 4);
        uint64_t major = 0, minor = 0;
        if (!s || std::memcmp(magic, "KERN", 4) || !r.number(major, 1) || !r.number(minor, 1) || major != 1 ||
            minor != 0)
        {
            error = "Unsupported bytecode format; rebuild scripts for version 1.0.";
            return false;
        }
        if (!r.string(p.moduleName))
            return false;
        uint64_t n = 0;
        if (!r.count(n, sizeof(std::string)))
            return false;
        p.strings.resize(static_cast<size_t>(n));
        for (auto &v : p.strings)
            if (!r.string(v))
                return false;
        if (!r.count(n, sizeof(ProgramExtern)))
            return false;
        p.externs.resize(static_cast<size_t>(n));
        for (auto &e : p.externs)
            if (!r.string(e.name) || !r.signature(e.signature))
                return false;
        if (!r.count(n, sizeof(ProgramFunction)))
            return false;
        p.functions.resize(static_cast<size_t>(n));
        for (auto &f : p.functions)
        {
            uint64_t slots = 0, count = 0;
            if (!r.string(f.name) || !r.signature(f.signature) || !r.number(slots) ||
                !r.count(count, sizeof(Instruction)))
                return false;
            f.frameSize = static_cast<uint32_t>(slots);
            f.instructions.resize(static_cast<size_t>(count));
            for (auto &i : f.instructions)
            {
                uint64_t op = 0;
                if (!r.number(op, 2) || op >= std::size(instructions))
                    return false;
                i.id = static_cast<Opcode>(op);
                for (size_t j = 0; j < instructions[op].argCount; ++j)
                {
                    uint64_t arg = 0;
                    if (!r.number(arg))
                        return false;
                    i.args[j] = std::bit_cast<int32_t>(static_cast<uint32_t>(arg));
                }
            }
        }
        return true;
    }
    catch (const std::bad_alloc &)
    {
        throw kern::RuntimeError("Loaded program allocation failed.");
    }
    catch (const std::length_error &)
    {
        throw kern::RuntimeError("Loaded program allocation exceeds the limit.");
    }
}
