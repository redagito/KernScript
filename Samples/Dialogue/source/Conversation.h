#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../../Support/Files.h"
#include <kern/Kern.h>

namespace dialogue
{
namespace fs = std::filesystem;
inline void text(const std::string &value, std::size_t limit, bool empty = false)
{
    if ((!empty && value.empty()) || value.size() > limit)
        throw std::runtime_error("Conversation text is empty or too long");
    (void)samples::quote(value);
}
inline void identifier(const std::string &value)
{
    text(value, 80);
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.'))
            throw std::runtime_error("Scenario, rule and state IDs use letters, digits, dots, underscores and hyphens");
}
struct Turn
{
    std::string input, response, rule;
    bool operator==(const Turn &) const = default;
};
struct Session
{
    std::string scenario;
    std::vector<Turn> turns;
    std::map<std::string, int> state;
    bool operator==(const Session &) const = default;
};
struct Context
{
    Turn turn;
    std::string scenario;
    std::map<std::string, int> state;
    int number = 0, replies = 0;
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
inline std::string input() { return active->turn.input; }
inline int number() { return active->number; }
inline std::string lower(std::string value)
{
    for (char &c : value)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return value;
}
inline int hasWord(std::string word)
{
    const auto wanted = lower(std::move(word)), message = lower(active->turn.input);
    if (wanted.empty())
        return 0;
    const auto wordChar = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
    std::size_t cursor = 0;
    while ((cursor = message.find(wanted, cursor)) != std::string::npos)
    {
        const auto end = cursor + wanted.size();
        if ((cursor == 0 || !wordChar(message[cursor - 1])) && (end == message.size() || !wordChar(message[end])))
            return 1;
        ++cursor;
    }
    return 0;
}
inline int stateValue(std::string name)
{
    identifier(name);
    const auto found = active->state.find(name);
    return found == active->state.end() ? 0 : found->second;
}
inline void setState(std::string name, int value)
{
    identifier(name);
    if (!active->state.contains(name) && active->state.size() == 128)
        throw std::runtime_error("A conversation supports at most 128 named state values");
    active->state[name] = value;
}
inline void identify(std::string scenario)
{
    identifier(scenario);
    if (!active->scenario.empty())
        throw std::runtime_error("A response must identify its scenario exactly once");
    active->scenario = std::move(scenario);
}
inline void reply(std::string rule, std::string response)
{
    identifier(rule);
    text(response, 8192);
    if (++active->replies != 1)
        throw std::runtime_error("A response must match exactly one rule");
    active->turn.rule = std::move(rule);
    active->turn.response = std::move(response);
}
class Logger final : public kern::ILogger
{
public:
    std::string messages;

protected:
    void onError(const std::string &message) override { messages += message + '\n'; }
};
class Conversation
{
    Logger m_logger;
    std::unique_ptr<kern::IVirtualMachine> m_vm;
    Session m_session;
    fs::path m_source, m_file;
    std::string m_saved;
    std::unique_ptr<kern::IVirtualMachine> compile(const fs::path &source)
    {
        m_logger.messages.clear();
        std::istringstream input(samples::readText(source, 1024 * 1024));
        std::stringstream bytecode;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&m_logger));
        if (!compiler->compileBytecode(input, bytecode, source.string(), {source.parent_path().string()}))
            throw std::runtime_error("Scenario compile failed:\n" + m_logger.messages);
        kern::VMConfig config;
        config.maxExecutionSteps = 100000;
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(&m_logger, config));
        vm->addFunction("input", &dialogue::input);
        vm->addFunction("turn_number", &number);
        vm->addFunction("has_word", &hasWord);
        vm->addFunction("state", &stateValue);
        vm->addFunction("set_state", &setState);
        vm->addFunction("scenario", &identify);
        vm->addFunction("reply", &reply);
        if (!vm->load(bytecode))
            throw std::runtime_error("Scenario bytecode could not be loaded");
        return vm;
    }
    static void append(kern::IVirtualMachine &vm, Session &session, const std::string &message)
    {
        text(message, 4096, session.turns.empty());
        if (session.turns.size() >= 1001)
            throw std::runtime_error("A conversation supports 1000 messages");
        Context context;
        context.turn.input = message;
        context.state = session.state;
        context.number = static_cast<int>(session.turns.size());
        Scope scope(context);
        if (!vm.callFunction("respond"))
            throw std::runtime_error("A scenario needs func respond() -> void");
        if (context.replies != 1 || context.scenario.empty())
            throw std::runtime_error("A response must identify its scenario and match one rule");
        if (!session.scenario.empty() && session.scenario != context.scenario)
            throw std::runtime_error("Scenario identity changed during the conversation");
        std::size_t bytes = context.turn.input.size() + context.turn.response.size();
        for (const auto &turn : session.turns)
            bytes += turn.input.size() + turn.response.size();
        if (bytes > 8 * 1024 * 1024)
            throw std::runtime_error("The conversation exceeds 8 MiB");
        session.scenario = std::move(context.scenario);
        session.state = std::move(context.state);
        session.turns.push_back(std::move(context.turn));
    }
    static Session replay(kern::IVirtualMachine &vm, const Session &old)
    {
        Session result;
        append(vm, result, "");
        if (!old.scenario.empty() && old.scenario != result.scenario)
            throw std::runtime_error("The saved conversation belongs to another scenario. Load its script first");
        for (std::size_t i = 1; i < old.turns.size(); ++i)
            append(vm, result, old.turns[i].input);
        return result;
    }
    static bool same(const fs::path &a, const fs::path &b)
    {
        return !b.empty() && (fs::weakly_canonical(a) == fs::weakly_canonical(b) ||
                              (fs::exists(a) && fs::exists(b) && fs::equivalent(a, b)));
    }

public:
    const Session &session() const { return m_session; }
    const fs::path &source() const { return m_source; }
    const fs::path &file() const { return m_file; }
    static std::string encode(const Session &session)
    {
        std::ostringstream out;
        out << "DIALOGUE 1\nscenario " << samples::quote(session.scenario) << '\n';
        for (std::size_t i = 1; i < session.turns.size(); ++i)
            out << "message " << samples::quote(session.turns[i].input) << '\n';
        return out.str();
    }
    bool dirty() const { return encode(m_session) != m_saved; }
    void start(const fs::path &source)
    {
        auto candidate = compile(source);
        auto session = replay(*candidate, {});
        m_vm = std::move(candidate);
        m_session = std::move(session);
        m_source = source;
        m_file.clear();
        m_saved = encode(m_session);
    }
    void reload()
    {
        auto candidate = compile(m_source);
        auto session = replay(*candidate, m_session);
        m_vm = std::move(candidate);
        m_session = std::move(session);
    }
    void send(const std::string &message)
    {
        if (message.empty())
            throw std::runtime_error("Enter a message first");
        auto candidate = m_session;
        append(*m_vm, candidate, message);
        m_session = std::move(candidate);
    }
    void undo()
    {
        if (m_session.turns.size() <= 1)
            return;
        auto shorter = m_session;
        shorter.turns.pop_back();
        m_session = replay(*m_vm, shorter);
    }
    void save(const fs::path &path)
    {
        samples::requireDataDestination(path);
        if (same(path, m_source))
            throw std::runtime_error("Choose a conversation path different from the scenario script");
        const auto encoded = encode(m_session);
        samples::writeAtomically(path, [&](std::ostream &out) { out << encoded; });
        m_file = path;
        m_saved = encoded;
    }
    void open(const fs::path &path)
    {
        auto encoded = samples::readText(path, 8 * 1024 * 1024);
        for (std::size_t i = 0; (i = encoded.find("\r\n", i)) != std::string::npos;)
            encoded.erase(i, 1);
        std::istringstream input(encoded);
        std::string line;
        if (!std::getline(input, line) || line != "DIALOGUE 1" || !std::getline(input, line) ||
            !line.starts_with("scenario "))
            throw std::runtime_error("Not a supported Dialogue conversation");
        Session saved;
        saved.scenario = samples::unquote(line.substr(9));
        identifier(saved.scenario);
        saved.turns.emplace_back();
        while (std::getline(input, line))
        {
            if (!line.starts_with("message "))
                throw std::runtime_error("Invalid saved message record");
            Turn turn;
            turn.input = samples::unquote(line.substr(8));
            text(turn.input, 4096);
            saved.turns.push_back(std::move(turn));
            if (saved.turns.size() > 1001)
                throw std::runtime_error("Too many saved messages");
        }
        auto candidate = replay(*m_vm, saved);
        m_session = std::move(candidate);
        m_file = path;
        m_saved = encode(m_session);
    }
    void exportTranscript(const fs::path &path) const
    {
        samples::requireDataDestination(path);
        if (same(path, m_file) || same(path, m_source))
            throw std::runtime_error("Choose a transcript path different from the saved conversation and script");
        samples::writeAtomically(path,
                                 [&](std::ostream &out)
                                 {
                                     out << "Scenario: " << m_session.scenario << '\n';
                                     for (const auto &turn : m_session.turns)
                                     {
                                         if (!turn.input.empty())
                                             out << "\nYou: " << turn.input << '\n';
                                         out << "Reply [" << turn.rule << "]: " << turn.response << '\n';
                                     }
                                 });
    }
};
} // namespace dialogue
