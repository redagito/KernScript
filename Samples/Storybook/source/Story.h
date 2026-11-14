#pragma once

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

#include "../../Support/Files.h"

namespace storybook
{
namespace fs = std::filesystem;

inline void text(const std::string &value, std::size_t limit, bool allowEmpty = false)
{
    if ((!allowEmpty && value.empty()) || value.size() > limit)
        throw std::runtime_error("Story text is empty or exceeds its limit");
    (void)samples::quote(value);
}

inline void identifier(const std::string &value)
{
    text(value, 80);
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '-'))
            throw std::runtime_error("Story identifiers use letters, digits, underscores, dots and hyphens");
}

struct Line
{
    std::string actor, words;
    bool operator==(const Line &) const = default;
};
struct Choice
{
    std::string id, label, target;
    bool operator==(const Choice &) const = default;
};
struct Frame
{
    std::string node, storyId, title, next, ending;
    std::vector<Line> lines;
    std::vector<Choice> choices;
    std::map<std::string, int> flags;
    bool operator==(const Frame &) const = default;
};
struct Action
{
    std::string source, id, target;
    bool operator==(const Action &) const = default;
};
struct Session
{
    std::string hero, storyId;
    std::vector<Action> actions;
    std::vector<Frame> frames;
    bool operator==(const Session &) const = default;
};

struct RenderContext
{
    Frame frame;
    std::string hero;
    std::size_t bytes = 0;
    void charge(const std::string &value)
    {
        text(value, 4096);
        bytes += value.size();
        if (bytes > 65536)
            throw std::runtime_error("A scene exceeds 64 KiB of output");
    }
};
inline RenderContext *active = nullptr;
class RenderScope
{
    RenderContext *previous;

public:
    explicit RenderScope(RenderContext &context) : previous(active) { active = &context; }
    ~RenderScope() { active = previous; }
    RenderScope(const RenderScope &) = delete;
    RenderScope &operator=(const RenderScope &) = delete;
};
inline std::string nodeValue() { return active->frame.node; }
inline std::string heroValue() { return active->hero; }
inline int flagValue(std::string name)
{
    identifier(name);
    const auto found = active->frame.flags.find(name);
    return found == active->frame.flags.end() ? 0 : found->second;
}
inline void flagSet(std::string name, int value)
{
    identifier(name);
    auto &flags = active->frame.flags;
    if (!flags.contains(name) && flags.size() == 256)
        throw std::runtime_error("A story supports at most 256 state flags");
    flags[name] = value;
}
inline void storyId(std::string id)
{
    identifier(id);
    if (!active->frame.storyId.empty())
        throw std::runtime_error("A scene must identify its story exactly once");
    active->frame.storyId = std::move(id);
}
inline void titleSet(std::string title)
{
    active->charge(title);
    if (!active->frame.title.empty())
        throw std::runtime_error("A scene must have exactly one title");
    active->frame.title = std::move(title);
}
inline void say(std::string actor, std::string words)
{
    active->charge(actor);
    active->charge(words);
    if (active->frame.lines.size() == 200)
        throw std::runtime_error("A scene supports at most 200 dialogue lines");
    active->frame.lines.push_back({std::move(actor), std::move(words)});
}
inline void choice(std::string id, std::string label, std::string target)
{
    identifier(id);
    identifier(target);
    active->charge(label);
    auto &choices = active->frame.choices;
    if (choices.size() == 3 || std::ranges::any_of(choices, [&](const Choice &c) { return c.id == id; }))
        throw std::runtime_error("A scene supports three choices with distinct IDs");
    choices.push_back({std::move(id), std::move(label), std::move(target)});
}
inline void next(std::string target)
{
    identifier(target);
    if (!active->frame.next.empty())
        throw std::runtime_error("A scene has more than one next destination");
    active->frame.next = std::move(target);
}
inline void finish(std::string ending)
{
    active->charge(ending);
    if (!active->frame.ending.empty())
        throw std::runtime_error("A scene has more than one ending");
    active->frame.ending = std::move(ending);
}

class Logger final : public kern::ILogger
{
public:
    std::string messages;

protected:
    void onError(const std::string &message) override { messages += message + '\n'; }
};

class Engine
{
    // The logger must outlive the VM that retains its address.
    Logger m_logger;
    std::unique_ptr<kern::IVirtualMachine> m_vm;
    fs::path m_source, m_file;
    Session m_session;
    std::string m_saved;

    static bool sameFile(const fs::path &a, const fs::path &b)
    {
        return !b.empty() && (fs::weakly_canonical(a) == fs::weakly_canonical(b) ||
                              (fs::exists(a) && fs::exists(b) && fs::equivalent(a, b)));
    }

    std::unique_ptr<kern::IVirtualMachine> compile(const fs::path &source)
    {
        m_logger.messages.clear();
        std::istringstream input(samples::readText(source, 1024 * 1024));
        std::stringstream bytecode;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&m_logger));
        if (!compiler->compileBytecode(input, bytecode, source.string(), {source.parent_path().string()}))
            throw std::runtime_error("Story compile failed:\n" + m_logger.messages);
        kern::VMConfig config;
        config.maxExecutionSteps = 100000;
        std::unique_ptr<kern::IVirtualMachine> candidate(kern::createVirtualMachine(&m_logger, config));
        candidate->addFunction("node", &nodeValue);
        candidate->addFunction("hero", &heroValue);
        candidate->addFunction("get_flag", &flagValue);
        candidate->addFunction("set_flag", &flagSet);
        candidate->addFunction("story_id", &storyId);
        candidate->addFunction("title", &titleSet);
        candidate->addFunction("say", &say);
        candidate->addFunction("choice", &choice);
        candidate->addFunction("next", &next);
        candidate->addFunction("finish", &finish);
        if (!candidate->load(bytecode))
            throw std::runtime_error("Story bytecode could not be loaded");
        return candidate;
    }

    static Frame render(kern::IVirtualMachine &vm, const std::string &node, const std::string &hero,
                        const std::map<std::string, int> &flags)
    {
        RenderContext context;
        context.frame.node = node;
        context.frame.flags = flags;
        context.hero = hero;
        RenderScope scope(context);
        if (!vm.callFunction("render"))
            throw std::runtime_error("The story needs a void render() entry point");
        const auto &frame = context.frame;
        if (frame.title.empty() || frame.storyId.empty() || frame.lines.empty())
            throw std::runtime_error("A scene needs a story ID, title and dialogue");
        const int exits = !frame.next.empty() + !frame.choices.empty() + !frame.ending.empty();
        if (exits != 1)
            throw std::runtime_error("A scene needs exactly one next destination, choice group or ending");
        return std::move(context.frame);
    }

    static std::string destination(const Frame &frame, const std::string &action)
    {
        if (action == "@next" && !frame.next.empty())
            return frame.next;
        for (const auto &choice : frame.choices)
            if (choice.id == action)
                return choice.target;
        throw std::runtime_error("Choose one of this scene's available actions");
    }

    static void validateSize(const Session &session)
    {
        if (session.actions.size() > 1000)
            throw std::runtime_error("A saved playthrough supports at most 1000 transitions");
        std::size_t size = 0;
        for (const auto &frame : session.frames)
        {
            size += frame.title.size() + frame.ending.size();
            for (const auto &line : frame.lines)
                size += line.actor.size() + line.words.size();
            // Account for state snapshots too, including flags left by earlier scenes.
            for (const auto &[key, value] : frame.flags)
                size += key.size() + sizeof(value);
            for (const auto &choice : frame.choices)
                size += choice.label.size() + choice.id.size() + choice.target.size();
        }
        if (size > 8 * 1024 * 1024)
            throw std::runtime_error("The story backlog exceeds 8 MiB");
    }

    static Session replay(kern::IVirtualMachine &vm, std::string hero, const std::string &expectedStory,
                          const std::vector<Action> &actions)
    {
        text(hero, 80);
        Session result;
        result.hero = std::move(hero);
        result.frames.push_back(render(vm, "start", result.hero, {}));
        result.storyId = result.frames.front().storyId;
        if (!expectedStory.empty() && result.storyId != expectedStory)
            throw std::runtime_error("This save belongs to a different story. Load its script first");
        for (const auto &action : actions)
        {
            const auto &frame = result.frames.back();
            if (frame.node != action.source || destination(frame, action.id) != action.target)
                throw std::runtime_error("The story's routes no longer match this playthrough");
            auto following = render(vm, action.target, result.hero, frame.flags);
            if (following.storyId != result.storyId)
                throw std::runtime_error("The story ID changed between scenes");
            result.actions.push_back(action);
            result.frames.push_back(std::move(following));
            validateSize(result);
        }
        return result;
    }

public:
    const Session &session() const { return m_session; }
    const Frame &current() const { return m_session.frames.back(); }
    const fs::path &source() const { return m_source; }
    const fs::path &file() const { return m_file; }

    static std::string serialize(const Session &session)
    {
        std::ostringstream out;
        out << "STORYBOOK 1\nhero " << samples::quote(session.hero) << "\nstory " << samples::quote(session.storyId)
            << '\n';
        for (const auto &action : session.actions)
            out << "action " << action.source << ' ' << action.id << ' ' << action.target << '\n';
        return out.str();
    }
    bool dirty() const { return serialize(m_session) != m_saved; }

    void start(const fs::path &source, const std::string &hero)
    {
        auto candidate = compile(source);
        auto session = replay(*candidate, hero, {}, {});
        m_vm = std::move(candidate);
        m_source = source;
        m_session = std::move(session);
        m_file.clear();
        m_saved.clear();
    }

    void restart(const std::string &hero) { start(m_source, hero); }

    void reload()
    {
        auto candidate = compile(m_source);
        auto session = replay(*candidate, m_session.hero, m_session.storyId, m_session.actions);
        m_vm = std::move(candidate);
        m_session = std::move(session);
    }

    void advance(const std::string &action)
    {
        if (m_session.actions.size() == 1000)
            throw std::runtime_error("This playthrough reached its 1000-transition limit");
        const auto &frame = current();
        const auto target = destination(frame, action);
        auto following = render(*m_vm, target, m_session.hero, frame.flags);
        if (following.storyId != m_session.storyId)
            throw std::runtime_error("The story ID changed between scenes");
        Session candidate = m_session;
        candidate.actions.push_back({frame.node, action, target});
        candidate.frames.push_back(std::move(following));
        validateSize(candidate);
        m_session = std::move(candidate);
    }

    void back()
    {
        if (m_session.actions.empty())
            return;
        m_session.actions.pop_back();
        m_session.frames.pop_back();
    }

    void save(const fs::path &path)
    {
        samples::requireDataDestination(path);
        if (sameFile(path, m_source))
            throw std::runtime_error("Choose a save path different from the story script");
        const auto encoded = serialize(m_session);
        samples::writeAtomically(path, [&](std::ostream &out) { out << encoded; });
        m_file = path;
        m_saved = encoded;
    }

    void open(const fs::path &path)
    {
        auto encoded = samples::readText(path, 256 * 1024);
        // Accept ordinary Windows editor line endings as well as our LF saves.
        for (std::size_t i = 0; (i = encoded.find("\r\n", i)) != std::string::npos;)
            encoded.erase(i, 1);
        std::istringstream input(encoded);
        std::string line, hero, story;
        if (!std::getline(input, line) || line != "STORYBOOK 1" || !std::getline(input, line) ||
            !line.starts_with("hero "))
            throw std::runtime_error("Not a supported Storybook save");
        hero = samples::unquote(line.substr(5));
        if (!std::getline(input, line) || !line.starts_with("story "))
            throw std::runtime_error("Missing save story ID");
        story = samples::unquote(line.substr(6));
        identifier(story);
        std::vector<Action> actions;
        while (std::getline(input, line))
        {
            std::istringstream row(line);
            std::string tag, extra;
            Action action;
            if (!(row >> tag >> action.source >> action.id >> action.target) || tag != "action" || (row >> extra))
                throw std::runtime_error("Invalid saved transition");
            identifier(action.source);
            identifier(action.target);
            if (action.id != "@next")
                identifier(action.id);
            actions.push_back(std::move(action));
            if (actions.size() > 1000)
                throw std::runtime_error("Too many saved transitions");
        }
        auto candidate = replay(*m_vm, hero, story, actions);
        m_session = std::move(candidate);
        m_file = path;
        m_saved = serialize(m_session);
    }

    void exportTranscript(const fs::path &path) const
    {
        samples::requireDataDestination(path);
        if (sameFile(path, m_file) || sameFile(path, m_source))
            throw std::runtime_error("Choose a transcript path different from the saved game and story script");
        samples::writeAtomically(path,
                                 [&](std::ostream &out)
                                 {
                                     out << m_session.storyId << "\nTraveler: " << m_session.hero << "\n";
                                     for (std::size_t i = 0; i < m_session.frames.size(); ++i)
                                     {
                                         const auto &frame = m_session.frames[i];
                                         out << "\n" << frame.title << "\n";
                                         for (const auto &line : frame.lines)
                                             out << line.actor << ": " << line.words << '\n';
                                         if (!frame.ending.empty())
                                             out << "Ending: " << frame.ending << '\n';
                                         if (i < m_session.actions.size())
                                             for (const auto &choice : frame.choices)
                                                 if (choice.id == m_session.actions[i].id)
                                                     out << "> " << choice.label << '\n';
                                     }
                                 });
    }
};
} // namespace storybook
