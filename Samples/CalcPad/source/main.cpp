#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <kern/Kern.h>

namespace fs = std::filesystem;
namespace
{
constexpr std::size_t kMaxLine = 64 * 1024;
constexpr std::size_t kMaxSession = 4 * 1024 * 1024;
constexpr std::size_t kMaxRecords = 10000;
constexpr uint64_t kStepBudget = 100000;
constexpr char kMagic[] = "KernScript CalcPad session 1";

struct Record
{
    std::string input;
    bool ok = false;
    double value = 0;
    std::string message;
    bool operator==(const Record &) const = default;
};
struct State
{
    std::map<std::string, double> variables;
    std::vector<std::string> imports;
    std::vector<Record> history;
};

class ConsoleLogger final : public kern::ILogger
{
    void onError(const std::string &message) override { std::cerr << message << '\n'; }
};

class Capture final : public kern::IExternFunction
{
public:
    explicit Capture(unsigned int arity = 1) : m_arity(arity) {}
    kern::FunctionSignature getSignature() const override { return {m_arity, kern::ReturnKind::Void}; }
    bool call(kern::IExternCall &call) override
    {
        if (call.getArgumentCount() != m_arity)
            return false;
        if (m_arity == 0)
        {
            ++m_count;
            return true;
        }
        double value = 0;
        if (!call.getArgument(0, value))
            return false;
        m_value = value;
        ++m_count;
        return true;
    }
    unsigned int count() const { return m_count; }
    double value() const { return m_value; }

private:
    unsigned int m_arity;
    unsigned int m_count = 0;
    double m_value = 0;
};

class Variables final : public kern::IExternFunction
{
    std::vector<float> m_values;

public:
    explicit Variables(std::vector<float> values) : m_values(std::move(values)) {}
    kern::FunctionSignature getSignature() const override { return {1, kern::ReturnKind::Value}; }
    bool call(kern::IExternCall &call) override
    {
        int index = -1;
        if (!call.getArgument(0, index))
            return false;
        if (index < 0 || index >= static_cast<int>(m_values.size()))
            throw kern::RuntimeError("Invalid calculation variable index");
        call.setReturnValue(m_values[index]);
        return true;
    }
};

std::string trim(std::string s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
bool identifier(std::string_view s)
{
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_'))
        return false;
    return std::all_of(s.begin() + 1, s.end(),
                       [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
}
bool reserved(std::string_view s)
{
    static const std::vector<std::string_view> words = {"func", "extern", "var", "if",  "else",   "while",  "return",
                                                        "and",  "or",     "xor", "not", "module", "import", "export"};
    return std::find(words.begin(), words.end(), s) != words.end() || s.rfind("__calcpad_", 0) == 0;
}
std::string literal(double v)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << v;
    std::string s = out.str();
    if (s.find_first_of(".eE") == std::string::npos)
        s += ".0";
    return s;
}
std::string floatLiteral(float v)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << v;
    std::string s = out.str();
    if (s.find_first_of(".eE") == std::string::npos)
        s += ".0";
    return s;
}
std::string quote(std::string_view s)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 32 || c == 127)
                return {};
            else
                out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}
bool parseQuoted(const std::string &s, std::size_t &position, std::string &out)
{
    if (position >= s.size() || s[position] != '"')
        return false;
    ++position;
    out.clear();
    while (position < s.size())
    {
        const char c = s[position++];
        if (c == '"')
            return true;
        if (c == '\\')
        {
            if (position >= s.size())
                return false;
            switch (s[position++])
            {
            case '\\':
                out += '\\';
                break;
            case '"':
                out += '"';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            default:
                return false;
            }
        }
        else
        {
            const auto u = static_cast<unsigned char>(c);
            if (u < 32 || u == 127)
                return false;
            out += c;
        }
    }
    return false;
}
bool finiteValue(const std::string &s, double &v)
{
    float parsed = 0.0f;
    const char *first = s.data(), *last = first + s.size();
    const auto result = std::from_chars(first, last, parsed, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != last || !std::isfinite(parsed))
        return false;
    v = parsed;
    return true;
}
bool finiteDoubleValue(const std::string &s, double &v)
{
    const char *first = s.data(), *last = first + s.size();
    const auto result = std::from_chars(first, last, v, std::chars_format::general);
    return result.ec == std::errc{} && result.ptr == last && std::isfinite(v) &&
           std::abs(v) <= std::numeric_limits<float>::max();
}

struct Pad
{
    ConsoleLogger logger;
    State state;
    std::vector<std::string> searchPaths;
    bool errors = false;

    bool calculateWith(const State &context, const std::string &expression, double &result, std::string &error)
    {
        if (expression.empty() || expression.size() > kMaxLine ||
            expression.find_first_of(";{}\n\r") != std::string::npos)
        {
            error = "enter one expression on a single line";
            return false;
        }
        std::ostringstream src;
        for (const auto &name : context.imports)
            src << "import " << name << ";\n";
        src << "extern __calcpad_capture(value) -> void;\n";
        src << "extern __calcpad_value(index) -> value;\n";
        src << "func __calcpad_main() -> void {\n";
        std::vector<float> values;
        for (const auto &[name, value] : context.variables)
        {
            src << "var " << name << " = __calcpad_value(" << values.size() << ");\n";
            values.push_back(static_cast<float>(value));
        }
        src << "__calcpad_capture((" << expression << "\n));\n}\n";
        std::istringstream in(src.str());
        std::ostringstream bytecode;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
        if (!compiler->compileBytecode(in, bytecode, "<CalcPad expression>", searchPaths))
        {
            error = "could not compile expression";
            return false;
        }
        kern::VMConfig config;
        config.maxExecutionSteps = kStepBudget;
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(&logger, config));
        auto ownedCapture = std::make_unique<Capture>();
        Capture *capture = ownedCapture.get();
        vm->addFunction("__calcpad_capture", capture);
        ownedCapture.release();
        auto variables = std::make_unique<Variables>(std::move(values));
        vm->addFunction("__calcpad_value", variables.get());
        variables.release();
        std::istringstream bytes(bytecode.str());
        try
        {
            if (!vm->load(bytes) || !vm->callFunction("__calcpad_main"))
            {
                error = "could not load calculation";
                return false;
            }
        }
        catch (const kern::RuntimeError &e)
        {
            error = e.what();
            return false;
        }
        if (capture->count() != 1)
        {
            error = "expression must produce exactly one result";
            return false;
        }
        result = capture->value();
        if (!std::isfinite(result))
        {
            error = "result is not finite";
            return false;
        }
        return true;
    }

    bool calculate(const std::string &expression, double &result, std::string &error)
    {
        return calculateWith(state, expression, result, error);
    }

    bool process(const std::string &raw, bool quiet = false)
    {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
            return true;
        if (line == ":quit" || line == ":q")
            return false;
        if (line == ":history")
        {
            for (std::size_t i = 0; i < state.history.size(); ++i)
            {
                const auto &r = state.history[i];
                std::cout << (i + 1) << ". " << r.input << " => ";
                if (r.ok)
                    std::cout << literal(r.value);
                else
                    std::cout << "error: " << r.message;
                std::cout << '\n';
            }
            return true;
        }
        if (line.rfind(":import ", 0) == 0)
        {
            const std::string name = trim(line.substr(8));
            if (!identifier(name) || reserved(name))
            {
                errors = true;
                std::cerr << "invalid import name\n";
                return true;
            }
            State candidate = state;
            if (std::find(candidate.imports.begin(), candidate.imports.end(), name) != candidate.imports.end())
                return true;
            if (candidate.imports.size() >= 1024)
            {
                errors = true;
                std::cerr << "import limit reached\n";
                return true;
            }
            candidate.imports.push_back(name);
            double ignored = 0;
            std::string importError;
            if (!calculateWith(candidate, "1.0", ignored, importError))
            {
                errors = true;
                std::cerr << "import rejected: " << name << '\n';
                return true;
            }
            state = std::move(candidate);
            if (!quiet)
                std::cout << "imported " << name << '\n';
            return true;
        }
        if (line.rfind(":save ", 0) == 0)
        {
            if (!save(trim(line.substr(6))))
            {
                errors = true;
                std::cerr << "could not save session\n";
            }
            else if (!quiet)
                std::cout << "session saved\n";
            return true;
        }
        if (line.rfind(":open ", 0) == 0)
        {
            std::string e;
            if (!open(trim(line.substr(6)), e))
            {
                errors = true;
                std::cerr << "could not open session: " << e << '\n';
            }
            else if (!quiet)
                std::cout << "session opened\n";
            return true;
        }
        std::string expr = line;
        std::string varName;
        bool assignment = false;
        if (line.rfind("let ", 0) == 0)
        {
            const auto eq = line.find('=');
            if (eq == std::string::npos)
            {
                errors = true;
                std::cerr << "expected: let name = expression\n";
                return true;
            }
            varName = trim(line.substr(4, eq - 4));
            expr = trim(line.substr(eq + 1));
            if (!identifier(varName) || reserved(varName))
            {
                errors = true;
                std::cerr << "invalid variable name\n";
                return true;
            }
            assignment = true;
        }
        double value = 0;
        std::string error;
        const bool ok = calculate(expr, value, error);
        state.history.push_back({line, ok, value, error});
        if (state.history.size() > kMaxRecords)
            state.history.erase(state.history.begin());
        if (!ok)
        {
            errors = true;
            std::cerr << error << '\n';
            return true;
        }
        if (assignment && !state.variables.count(varName) && state.variables.size() >= kMaxRecords)
        {
            errors = true;
            std::cerr << "variable limit reached\n";
            return true;
        }
        if (assignment)
            state.variables[varName] = static_cast<float>(value);
        if (!quiet)
            std::cout << (assignment ? varName + " = " + floatLiteral(static_cast<float>(value))
                                     : "= " + literal(value))
                      << '\n';
        return true;
    }

    bool save(const std::string &path)
    {
        if (path.empty() || state.history.size() > kMaxRecords || state.variables.size() > kMaxRecords ||
            state.imports.size() > 1024)
            return false;
        fs::path target(path);
        fs::path temp;
        try
        {
            std::random_device random;
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int attempt = 0; attempt < 32; ++attempt)
            {
                temp = target.parent_path() / (target.filename().string() + ".calcpad-" + std::to_string(stamp) + "-" +
                                               std::to_string(random()) + ".tmp");
                std::error_code existsError;
                if (!fs::exists(temp, existsError) && !existsError)
                    break;
                temp.clear();
            }
        }
        catch (...)
        {
            return false;
        }
        if (temp.empty())
            return false;
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out << kMagic << '\n';
        out << "I " << state.imports.size() << '\n';
        for (const auto &i : state.imports)
        {
            const auto q = quote(i);
            if (q.empty())
            {
                out.close();
                fs::remove(temp);
                return false;
            }
            out << "I " << q << '\n';
        }
        out << "V " << state.variables.size() << '\n';
        for (const auto &[n, v] : state.variables)
        {
            const auto q = quote(n);
            if (q.empty() || !std::isfinite(v) || std::abs(v) > std::numeric_limits<float>::max())
            {
                out.close();
                fs::remove(temp);
                return false;
            }
            out << "V " << q << ' ' << floatLiteral(static_cast<float>(v)) << '\n';
        }
        out << "H " << state.history.size() << '\n';
        for (const auto &r : state.history)
        {
            const auto input = quote(r.input), message = quote(r.message);
            if (input.empty() || message.empty() || !std::isfinite(r.value) ||
                std::abs(r.value) > std::numeric_limits<float>::max())
            {
                out.close();
                fs::remove(temp);
                return false;
            }
            out << "H " << (r.ok ? 1 : 0) << ' ' << literal(r.value) << ' ' << input << ' ' << message << '\n';
        }
        out.flush();
        if (!out || static_cast<std::size_t>(out.tellp()) > kMaxSession)
        {
            out.close();
            fs::remove(temp);
            return false;
        }
        out.close();
        if (out.fail())
        {
            fs::remove(temp);
            return false;
        }
        std::error_code ec;
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            fs::remove(temp, ec);
            return false;
        }
#else
        fs::rename(temp, target, ec);
        if (ec)
        {
            fs::remove(temp);
            return false;
        }
#endif
        return true;
    }

    bool open(const std::string &path, std::string &error)
    {
        std::error_code ec;
        auto size = fs::file_size(path, ec);
        if (ec || size > kMaxSession)
        {
            error = "missing or oversized file";
            return false;
        }
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            error = "file cannot be read";
            return false;
        }
        State candidate;
        std::string line;
        if (!std::getline(in, line) || line != kMagic)
        {
            error = "unsupported session format";
            return false;
        }
        std::size_t count = 0;
        std::string value;
        if (!readCount(in, 'I', count, 1024))
        {
            error = "invalid import table";
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!readLine(in, line) || line.rfind("I ", 0) != 0)
            {
                error = "invalid import entry";
                return false;
            }
            std::size_t pos = 2;
            if (!parseQuoted(line, pos, value) || pos != line.size() || !identifier(value) || reserved(value))
            {
                error = "invalid import entry";
                return false;
            }
            if (std::find(candidate.imports.begin(), candidate.imports.end(), value) != candidate.imports.end())
            {
                error = "duplicate import";
                return false;
            }
            candidate.imports.push_back(value);
        }
        if (!readCount(in, 'V', count, kMaxRecords))
        {
            error = "invalid variable table";
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!readLine(in, line) || line.rfind("V ", 0) != 0)
            {
                error = "invalid variable entry";
                return false;
            }
            std::size_t pos = 2;
            std::string n, number;
            double parsed = 0;
            if (!parseQuoted(line, pos, n) || !identifier(n) || reserved(n) || !skipOneSpace(line, pos) ||
                !readToken(line, pos, number) || pos != line.size() || !finiteValue(number, parsed) ||
                candidate.variables.count(n))
            {
                error = "invalid variable value";
                return false;
            }
            candidate.variables[n] = parsed;
        }
        if (!readCount(in, 'H', count, kMaxRecords))
        {
            error = "invalid history table";
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!readLine(in, line) || line.rfind("H ", 0) != 0)
            {
                error = "invalid history entry";
                return false;
            }
            std::size_t pos = 2;
            std::string flag, number, input, message;
            double parsed = 0;
            if (!readToken(line, pos, flag) || (flag != "0" && flag != "1") || !skipOneSpace(line, pos) ||
                !readToken(line, pos, number) || !finiteDoubleValue(number, parsed) || !skipOneSpace(line, pos) ||
                !parseQuoted(line, pos, input) || !skipOneSpace(line, pos) || !parseQuoted(line, pos, message) ||
                pos != line.size())
            {
                error = "invalid history record";
                return false;
            }
            candidate.history.push_back({input, flag == "1", parsed, message});
        }
        if (std::getline(in, line) || !in.eof() || in.bad())
        {
            error = "trailing session data or read error";
            return false;
        }
        double ignored = 0;
        std::string validationError;
        if (!calculateWith(candidate, "1.0", ignored, validationError))
        {
            error = "session imports cannot be compiled";
            return false;
        }
        state = std::move(candidate);
        return true;
    }

    static bool readCount(std::istream &in, char tag, std::size_t &count, std::size_t max)
    {
        std::string line;
        if (!readLine(in, line) || line.size() < 3 || line[0] != tag || line[1] != ' ')
            return false;
        std::size_t n = 0;
        const char *first = line.data() + 2, *last = line.data() + line.size();
        const auto result = std::from_chars(first, last, n);
        if (result.ec != std::errc{} || result.ptr != last || n > max)
            return false;
        count = n;
        return true;
    }
    static bool readLine(std::istream &in, std::string &line)
    {
        return static_cast<bool>(std::getline(in, line)) && line.size() <= kMaxLine * 4;
    }
    static bool skipOneSpace(const std::string &s, std::size_t &position)
    {
        if (position >= s.size() || s[position] != ' ')
            return false;
        ++position;
        return true;
    }
    static bool readToken(const std::string &s, std::size_t &position, std::string &token)
    {
        const std::size_t start = position;
        while (position < s.size() && s[position] != ' ')
            ++position;
        if (start == position)
            return false;
        token = s.substr(start, position - start);
        return true;
    }
};

void help()
{
    std::cout << "CalcPad - KernScript calculation workpad\n"
                 "Usage: CalcPad [--help] [--verify] [--run FILE] [--library DIR] [--open FILE]\n"
                 "REPL: enter an expression, 'let name = expression', :history, :import MODULE, :save FILE, :open "
                 "FILE, :quit\n"
                 "Batch files use the same one-command-per-line syntax as the REPL.\n";
}

int verify(const fs::path &exeDir, const fs::path &fixtureRoot)
{
    Pad p;
    p.searchPaths.push_back((exeDir / "scripts").string());
    p.searchPaths.push_back((fs::current_path() / "scripts").string());
    std::error_code ec;
    fs::create_directories(fixtureRoot, ec);
    if (ec)
    {
        std::cerr << "verify fixture root unavailable\n";
        return 1;
    }
    std::random_device random;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path base = fixtureRoot / (std::to_string(stamp) + "-" + std::to_string(random()));
    fs::create_directories(base, ec);
    if (ec)
    {
        std::cerr << "verify fixture directory unavailable\n";
        return 1;
    }
    std::string error;
    double v = 0;
    auto check = [&](const std::string &e, double expect)
    {
        if (!p.calculate(e, v, error) || std::abs(v - expect) > 1e-4)
        {
            std::cerr << "verify failed: " << e << " " << error << '\n';
            return false;
        }
        return true;
    };
    if (!check("2 + 3 * 4", 14))
        return 1;
    if (!p.process(":import math_helpers", true) || p.state.imports != std::vector<std::string>{"math_helpers"})
    {
        std::cerr << "helper import failed\n";
        return 1;
    }
    p.process("let rate = 0.2", true);
    if (!check("100 * rate", 20))
        return 1;
    for (const auto &[name, number] :
         std::vector<std::pair<std::string, float>>{{"tiny", std::numeric_limits<float>::denorm_min()},
                                                    {"huge", std::numeric_limits<float>::max()},
                                                    {"negative_zero", -0.0f}})
    {
        p.state.variables[name] = number;
        if (!p.calculate(name, v, error) || v != static_cast<double>(number) ||
            (number == 0.0f && std::signbit(v) != std::signbit(number)))
        {
            std::cerr << "host variable did not preserve float32 range or signed zero\n";
            return 1;
        }
    }
    auto vars = p.state.variables;
    if (p.process("let rate = 1 / 0", true), p.state.variables != vars)
    {
        std::cerr << "failure changed variable state\n";
        return 1;
    }
    if (p.state.history.empty() || p.state.history.back().ok)
    {
        std::cerr << "failure missing from history\n";
        return 1;
    }
    const auto importsBeforeBad = p.state.imports;
    p.process(":import no_such_calcpad_module", true);
    if (p.state.imports != importsBeforeBad)
    {
        std::cerr << "failed import changed state\n";
        return 1;
    }
    const auto variablesBeforeStatement = p.state.variables;
    if (p.calculate("1); let hacked = 3", v, error) || p.state.variables != variablesBeforeStatement)
    {
        std::cerr << "statement injection was accepted\n";
        return 1;
    }
    if (!check("math_helpers::percent(250, 12)", 30))
        return 1;
    p.process("1 + 2 // quote \" and slash \\", true);
    const fs::path file = base / "roundtrip.calc";
    const fs::path protectedFile = base / "roundtrip.calc.calcpad-backup";
    {
        std::ofstream sentinel(protectedFile, std::ios::binary);
        sentinel << "keep this user file";
    }
    if (!p.save(file.string()))
    {
        std::cerr << "save failed\n";
        return 1;
    }
    p.process("let revision = 2", true);
    if (!p.save(file.string()))
    {
        std::cerr << "overwrite save failed\n";
        return 1;
    }
    {
        std::ifstream sentinel(protectedFile, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(sentinel)), {});
        if (content != "keep this user file")
        {
            std::cerr << "save touched a neighboring user file\n";
            return 1;
        }
    }
    Pad q;
    q.searchPaths = p.searchPaths;
    if (!q.open(file.string(), error) || q.state.variables != p.state.variables || q.state.history != p.state.history ||
        q.state.imports != p.state.imports || !q.calculate("100 * rate", v, error) || std::abs(v - 20) > 1e-4)
    {
        std::cerr << "roundtrip failed " << error << '\n';
        return 1;
    }
    const State beforeBadOpen = q.state;
    const fs::path malformed = base / "malformed.calc";
    const std::string badContents = std::string(kMagic) + "\nI 00garbage\n";
    {
        std::ofstream out(malformed, std::ios::binary);
        out << badContents;
    }
    if (q.open(malformed.string(), error) || q.state.variables != beforeBadOpen.variables ||
        q.state.history != beforeBadOpen.history || q.state.imports != beforeBadOpen.imports)
    {
        std::cerr << "malformed open changed current session\n";
        return 1;
    }
    {
        std::ifstream in(malformed, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), {});
        if (content != badContents)
        {
            std::cerr << "failed open modified source file\n";
            return 1;
        }
    }
    const fs::path malformedQuote = base / "malformed-quote.calc";
    {
        std::ofstream out(malformedQuote, std::ios::binary);
        out << kMagic << "\nI 0\nV 1\nV \"bad\"quote\" 1.0\nH 0\n";
    }
    if (q.open(malformedQuote.string(), error) || q.state.variables != beforeBadOpen.variables)
    {
        std::cerr << "malformed quote accepted or changed state\n";
        return 1;
    }
    const fs::path badFloat = base / "bad-float.calc";
    {
        std::ofstream out(badFloat, std::ios::binary);
        out << kMagic << "\nI 0\nV 1\nV \"huge\" 1e100\nH 0\n";
    }
    if (q.open(badFloat.string(), error) || q.state.variables != beforeBadOpen.variables)
    {
        std::cerr << "out-of-range float accepted\n";
        return 1;
    }
    const fs::path duplicate = base / "duplicate.calc";
    {
        std::ofstream out(duplicate, std::ios::binary);
        out << kMagic << "\nI 0\nV 2\nV \"same\" 1.0\nV \"same\" 2.0\nH 0\n";
    }
    if (q.open(duplicate.string(), error) || q.state.variables != beforeBadOpen.variables)
    {
        std::cerr << "duplicate variable accepted\n";
        return 1;
    }
    const fs::path invalidImport = base / "invalid-import.calc";
    {
        std::ofstream out(invalidImport, std::ios::binary);
        out << kMagic << "\nI 1\nI \"no_such_calcpad_module\"\nV 0\nH 0\n";
    }
    if (q.open(invalidImport.string(), error) || q.state.variables != beforeBadOpen.variables ||
        q.state.imports != beforeBadOpen.imports)
    {
        std::cerr << "invalid imported session was accepted\n";
        return 1;
    }
    const fs::path batch = base / "batch.txt";
    {
        std::ofstream out(batch);
        out << "let x = 6\nx * 7\n:quit\nlet after_quit = 1\n";
    }
    Pad b;
    std::ifstream in(batch);
    std::string line;
    while (std::getline(in, line))
        if (!b.process(line, true))
            break;
    if (b.state.history.size() != 2 || !b.state.history.back().ok || b.state.history.back().value != 42 ||
        b.state.variables.count("after_quit"))
    {
        std::cerr << "batch failed\n";
        return 1;
    }
    Pad estimate;
    estimate.searchPaths = p.searchPaths;
    std::ifstream renovation(exeDir / "scripts" / "renovation.txt");
    if (!renovation)
    {
        std::cerr << "bundled renovation batch is missing\n";
        return 1;
    }
    while (std::getline(renovation, line))
        if (!estimate.process(line, true))
            break;
    if (estimate.errors || std::abs(estimate.state.variables.at("economy_total") - 1665.86584f) > 0.02f ||
        std::abs(estimate.state.variables.at("standard_total") - 1986.54712f) > 0.02f ||
        std::abs(estimate.state.variables.at("premium_total") - 2444.66309f) > 0.02f)
    {
        std::cerr << "renovation quote comparison failed\n";
        return 1;
    }
    std::cout << "CalcPad verify passed\n";
    return 0;
}
} // namespace

int runCli(int argc, char **argv)
{
    Pad pad;
    const fs::path exeDir = argc > 0 ? fs::absolute(argv[0]).parent_path() : fs::current_path();
    pad.searchPaths.push_back((exeDir / "scripts").string());
    pad.searchPaths.push_back((fs::current_path() / "scripts").string());
    std::string runFile, openFile;
    bool verifyRequested = false;
    fs::path fixtureRoot = fs::current_path() / "build" / "test-tmp" / "CalcPad";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h")
        {
            help();
            return 0;
        }
        if (a == "--verify")
        {
            verifyRequested = true;
            continue;
        }
        if ((a == "--run" || a == "--open" || a == "--library" || a == "--test-root") && i + 1 < argc)
        {
            const std::string value = argv[++i];
            if (a == "--run")
                runFile = value;
            else if (a == "--open")
                openFile = value;
            else if (a == "--test-root")
                fixtureRoot = value;
            else
                pad.searchPaths.push_back(value);
        }
        else
        {
            help();
            return 2;
        }
    }
    if (verifyRequested)
        return verify(exeDir, fixtureRoot);
    if (!openFile.empty())
    {
        std::string e;
        if (!pad.open(openFile, e))
        {
            std::cerr << "could not open session: " << e << '\n';
            return 1;
        }
    }
    if (!runFile.empty())
    {
        std::error_code ec;
        auto n = fs::file_size(runFile, ec);
        if (ec || n > kMaxSession)
        {
            std::cerr << "batch file missing or too large\n";
            return 1;
        }
        std::ifstream in(runFile);
        if (!in)
        {
            std::cerr << "batch file cannot be read\n";
            return 1;
        }
        std::string line;
        std::size_t lines = 0;
        while (std::getline(in, line))
        {
            if (line.size() > kMaxLine || ++lines > kMaxRecords)
            {
                std::cerr << "batch limits exceeded\n";
                return 1;
            }
            if (!pad.process(line))
                break;
        }
        if (in.bad())
        {
            std::cerr << "batch read failed\n";
            return 1;
        }
        return pad.errors ? 1 : 0;
    }
    std::cout << "CalcPad. Type :help for commands, :quit to leave.\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line))
    {
        if (line.size() > kMaxLine)
        {
            std::cerr << "line too long\n";
            continue;
        }
        if (line == ":help")
        {
            help();
            continue;
        }
        if (!pad.process(line))
            break;
    }
    return 0;
}

int main(int argc, char **argv)
{
    try
    {
        return runCli(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << "CalcPad: " << e.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "CalcPad: unexpected failure\n";
        return 1;
    }
}
