#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <kern/Kern.h>

#include "model.hpp"

namespace daybook
{
struct Ranked
{
    std::size_t index;
    int score;
    std::string reason;
};

struct CallContext
{
    const Task *task = nullptr;
    int dueDays = 9999;
    int score = 0;
    int scoreEmissions = 0;
    std::vector<std::string> explanations;
};

inline CallContext *activeContext = nullptr;

class ContextScope
{
public:
    explicit ContextScope(CallContext &context) : m_previous(activeContext) { activeContext = &context; }
    ~ContextScope() { activeContext = m_previous; }
    ContextScope(const ContextScope &) = delete;
    ContextScope &operator=(const ContextScope &) = delete;

private:
    CallContext *m_previous;
};

inline int priorityValue() { return activeContext->task->priority; }
inline std::string kindValue() { return activeContext->task->kind; }
inline std::string tagsValue() { return activeContext->task->tags; }
inline bool hasTag(std::string wanted)
{
    std::istringstream input(activeContext->task->tags);
    std::string tag;
    while (std::getline(input, tag, ','))
    {
        const auto first = tag.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;
        const auto last = tag.find_last_not_of(" \t");
        if (tag.substr(first, last - first + 1) == wanted)
            return true;
    }
    return false;
}
inline int minutesValue() { return activeContext->task->minutes; }
inline int completedValue() { return activeContext->task->completed ? 1 : 0; }
inline int dueDaysValue() { return activeContext->dueDays; }
inline void emitScore(int score)
{
    activeContext->score = score;
    ++activeContext->scoreEmissions;
}
inline void emitExplanation(std::string text)
{
    if (!text.empty())
        activeContext->explanations.push_back(std::move(text));
}

inline std::chrono::year_month_day localToday()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {std::chrono::year{local.tm_year + 1900}, std::chrono::month{static_cast<unsigned>(local.tm_mon + 1)},
            std::chrono::day{static_cast<unsigned>(local.tm_mday)}};
}

inline int daysUntil(const Task &task, std::chrono::year_month_day today)
{
    if (task.deadline.empty())
        return 9999;
    const int year = std::stoi(task.deadline.substr(0, 4));
    const auto month = static_cast<unsigned>(std::stoi(task.deadline.substr(5, 2)));
    const auto day = static_cast<unsigned>(std::stoi(task.deadline.substr(8, 2)));
    const std::chrono::year_month_day deadline{std::chrono::year{year}, std::chrono::month{month},
                                               std::chrono::day{day}};
    if (!deadline.ok() || !today.ok())
        return 9999;
    return static_cast<int>((std::chrono::sys_days{deadline} - std::chrono::sys_days{today}).count());
}

class Logger final : public kern::ILogger
{
public:
    std::string messages;

protected:
    void onError(const std::string &message) override
    {
        messages += message + '\n';
        std::cerr << "Daybook policy: " << message << '\n';
    }
};

class Ranker
{
public:
    explicit Ranker(std::chrono::year_month_day today = localToday()) : m_today(today) {}

    bool reload(const fs::path &path, const Model &book, std::string &error)
    {
        m_logger.messages.clear();
        std::string text;
        try
        {
            text = samples::readText(path, 1024 * 1024);
        }
        catch (const std::exception &exception)
        {
            error = exception.what();
            return false;
        }
        std::istringstream source(text);

        std::stringstream assembly;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&m_logger));
        const std::vector<std::string> searchPaths = {path.parent_path().string()};
        if (!compiler->compile(source, assembly, path.string(), searchPaths))
        {
            error = m_logger.messages.empty() ? "Policy compile failed" : m_logger.messages;
            return false;
        }
        std::stringstream bytecode;
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler(&m_logger));
        if (!assembler->assemble(assembly, bytecode))
        {
            error = m_logger.messages.empty() ? "Policy assembly failed" : m_logger.messages;
            return false;
        }

        auto candidate = makeVm();
        try
        {
            if (!candidate->load(bytecode))
            {
                error = "Policy load failed";
                return false;
            }
            std::vector<Ranked> probe;
            if (!rankWith(*candidate, book, probe, error))
                return false;
        }
        catch (const kern::RuntimeError &exception)
        {
            error = exception.what();
            return false;
        }
        m_vm = std::move(candidate);
        m_active = path;
        return true;
    }

    bool rank(const Model &book, std::vector<Ranked> &out, std::string &error) const
    {
        if (!m_vm)
        {
            error = "No ranking policy is loaded";
            return false;
        }
        return rankWith(*m_vm, book, out, error);
    }

    const fs::path &activePolicy() const { return m_active; }
    void setToday(std::chrono::year_month_day date) { m_today = date; }

private:
    std::unique_ptr<kern::IVirtualMachine> makeVm()
    {
        kern::VMConfig config;
        config.maxExecutionSteps = 100'000;
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(&m_logger, config));
        vm->addFunction("priority", &priorityValue);
        vm->addFunction("kind", &kindValue);
        vm->addFunction("tags", &tagsValue);
        vm->addFunction("has_tag", &hasTag);
        vm->addFunction("minutes", &minutesValue);
        vm->addFunction("completed", &completedValue);
        vm->addFunction("due_days", &dueDaysValue);
        vm->addFunction("emit", &emitScore);
        vm->addFunction("explain", &emitExplanation);
        return vm;
    }

    bool rankWith(kern::IVirtualMachine &vm, const Model &book, std::vector<Ranked> &out, std::string &error) const
    {
        std::vector<Task> probeTask;
        const std::vector<Task> *tasks = &book.tasks();
        if (tasks->empty())
        {
            Task probe;
            probe.id = 1;
            probe.title = "Policy validation probe";
            probeTask.push_back(std::move(probe));
            tasks = &probeTask;
        }

        std::vector<Ranked> candidate;
        candidate.reserve(tasks->size());
        for (std::size_t index = 0; index < tasks->size(); ++index)
        {
            CallContext context;
            context.task = &(*tasks)[index];
            context.dueDays = daysUntil(*context.task, m_today);
            try
            {
                ContextScope scope(context);
                if (!vm.callFunction("rank"))
                {
                    error = "Policy function rank() is missing";
                    return false;
                }
            }
            catch (const kern::RuntimeError &exception)
            {
                error = exception.what();
                return false;
            }
            if (context.scoreEmissions != 1 || context.explanations.empty())
            {
                error = "Policy must emit exactly one score and at least one explanation per task";
                return false;
            }
            std::string reason;
            for (const auto &part : context.explanations)
            {
                if (!reason.empty())
                    reason += "; ";
                reason += part;
            }
            candidate.push_back({index, context.score, std::move(reason)});
        }
        std::stable_sort(candidate.begin(), candidate.end(),
                         [](const Ranked &left, const Ranked &right) { return left.score > right.score; });
        if (book.tasks().empty())
            candidate.clear();
        out = std::move(candidate);
        return true;
    }

    Logger m_logger;
    std::unique_ptr<kern::IVirtualMachine> m_vm;
    fs::path m_active;
    std::chrono::year_month_day m_today;
};
} // namespace daybook
