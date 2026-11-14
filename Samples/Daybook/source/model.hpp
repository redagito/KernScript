#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../../Support/Files.h"

namespace daybook
{
namespace fs = std::filesystem;

struct Task
{
    uint64_t id = 0;
    std::string title;
    std::string kind = "task";
    std::string tags;
    int priority = 3;
    int minutes = 30;
    std::string deadline;
    bool completed = false;
    bool operator==(const Task &) const = default;
};

inline bool validDate(const std::string &value)
{
    if (value.empty())
        return true;
    if (value.size() != 10 || value[4] != '-' || value[7] != '-')
        return false;
    for (std::size_t i = 0; i < value.size(); ++i)
        if (i != 4 && i != 7 && !std::isdigit(static_cast<unsigned char>(value[i])))
            return false;
    const int year = std::stoi(value.substr(0, 4));
    const unsigned int month = static_cast<unsigned int>(std::stoi(value.substr(5, 2)));
    const unsigned int day = static_cast<unsigned int>(std::stoi(value.substr(8, 2)));
    return std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}}.ok();
}

inline bool validTask(const Task &task, bool requireId = false)
{
    const auto supportedText = [](const std::string &text)
    {
        return std::all_of(text.begin(), text.end(),
                           [](unsigned char c) { return c >= 32 || c == '\n' || c == '\r' || c == '\t'; }) &&
               std::all_of(text.begin(), text.end(), [](unsigned char c) { return c != 127; });
    };
    return (!requireId || task.id != 0) && !task.title.empty() && task.title.size() <= 512 && task.kind.size() <= 64 &&
           task.tags.size() <= 512 && supportedText(task.title) && supportedText(task.kind) &&
           supportedText(task.tags) && task.priority >= 1 && task.priority <= 5 && task.minutes >= 0 &&
           task.minutes <= 1'000'000 && validDate(task.deadline);
}

template <typename Integer> inline bool parseInteger(const std::string &text, Integer &value)
{
    if (text.empty())
        return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

inline bool takeQuoted(const std::string &line, std::size_t &position, std::string &value)
{
    if (position >= line.size() || line[position] != '"')
        return false;
    const std::size_t start = position++;
    bool escaped = false;
    while (position < line.size())
    {
        const char c = line[position++];
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (c == '\\')
        {
            escaped = true;
            continue;
        }
        if (c == '"')
        {
            value = samples::unquote(line.substr(start, position - start));
            return true;
        }
    }
    return false;
}

class Model
{
public:
    static constexpr std::size_t maxTasks = 50'000;
    static constexpr std::size_t maxFileBytes = 8 * 1024 * 1024;

    const std::vector<Task> &tasks() const { return m_tasks; }
    uint64_t nextId() const { return m_nextId; }
    bool dirty() const { return m_dirty; }
    const fs::path &file() const { return m_file; }
    void clearDirty() { m_dirty = false; }

    uint64_t add(Task task)
    {
        validateForMutation(task);
        if (m_tasks.size() >= maxTasks)
            throw std::length_error("Task limit reached");
        if (m_nextId == 0 || m_nextId == std::numeric_limits<uint64_t>::max())
            throw std::overflow_error("Task ID space exhausted");
        task.id = m_nextId;
        const auto bytes = recordBytes(task);
        if (bytes > maxFileBytes - 128 - m_encodedBytes)
            throw std::length_error("Task document has reached its 8 MiB storage limit");
        m_tasks.push_back(std::move(task));
        m_encodedBytes += bytes;
        ++m_nextId;
        m_dirty = true;
        return m_tasks.back().id;
    }

    bool edit(uint64_t id, Task replacement)
    {
        validateForMutation(replacement);
        auto it = find(id);
        if (it == m_tasks.end())
            return false;
        replacement.id = id;
        const auto nextBytes = m_encodedBytes - recordBytes(*it) + recordBytes(replacement);
        if (nextBytes > maxFileBytes - 128)
            throw std::length_error("Task document has reached its 8 MiB storage limit");
        *it = std::move(replacement); // Full record replacement, including completion state.
        m_encodedBytes = nextBytes;
        m_dirty = true;
        return true;
    }

    bool erase(uint64_t id)
    {
        auto it = find(id);
        if (it == m_tasks.end())
            return false;
        m_encodedBytes -= recordBytes(*it);
        m_tasks.erase(it);
        m_dirty = true;
        return true;
    }

    bool complete(uint64_t id, bool value)
    {
        auto it = find(id);
        if (it == m_tasks.end())
            return false;
        it->completed = value;
        m_dirty = true;
        return true;
    }

    void save(const fs::path &path)
    {
        if (path.empty())
            throw std::invalid_argument("A save path is required");
        validateCollection(m_tasks, m_nextId);
        fs::path savedPath = path;
        samples::writeAtomically(path,
                                 [&](std::ostream &out)
                                 {
                                     out << "KernScript Daybook 1\n" << m_tasks.size() << ' ' << m_nextId << '\n';
                                     for (const auto &task : m_tasks)
                                         out << task.id << ' ' << task.priority << ' ' << task.minutes << ' '
                                             << (task.completed ? 1 : 0) << ' ' << samples::quote(task.title) << ' '
                                             << samples::quote(task.kind) << ' ' << samples::quote(task.tags) << ' '
                                             << samples::quote(task.deadline) << '\n';
                                 });
        m_file = std::move(savedPath);
        m_dirty = false;
    }

    void open(const fs::path &path)
    {
        const std::string contents = samples::readText(path, maxFileBytes);
        std::istringstream input(contents);
        std::string line;
        if (!std::getline(input, line) || line != "KernScript Daybook 1" || !std::getline(input, line))
            throw std::runtime_error("Invalid Daybook header");
        std::istringstream header(line);
        std::size_t count = 0;
        uint64_t next = 0;
        std::string countToken;
        std::string nextToken;
        std::string extra;
        if (!(header >> countToken >> nextToken) || (header >> extra) || !parseInteger(countToken, count) ||
            !parseInteger(nextToken, next) || count > maxTasks || next == 0)
            throw std::runtime_error("Invalid Daybook task count or next ID");

        std::vector<Task> candidate;
        candidate.reserve(count);
        std::set<uint64_t> ids;
        uint64_t largestId = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!std::getline(input, line) || line.size() > 4096)
                throw std::runtime_error("Missing or oversized task record");
            std::istringstream row(line);
            Task task;
            unsigned int complete = 0;
            std::string idToken;
            std::string priorityToken;
            std::string minutesToken;
            std::string completeToken;
            if (!(row >> idToken >> priorityToken >> minutesToken >> completeToken) || row.peek() != ' ' ||
                !parseInteger(idToken, task.id) || !parseInteger(priorityToken, task.priority) ||
                !parseInteger(minutesToken, task.minutes) || !parseInteger(completeToken, complete) || complete > 1)
                throw std::runtime_error("Invalid task fields");
            row.get();
            std::size_t position = static_cast<std::size_t>(row.tellg());
            if (!takeQuoted(line, position, task.title) || position >= line.size() || line[position++] != ' ' ||
                !takeQuoted(line, position, task.kind) || position >= line.size() || line[position++] != ' ' ||
                !takeQuoted(line, position, task.tags) || position >= line.size() || line[position++] != ' ' ||
                !takeQuoted(line, position, task.deadline) || position != line.size())
                throw std::runtime_error("Invalid quoted task field");
            task.completed = complete != 0;
            if (!validTask(task, true) || !ids.insert(task.id).second)
                throw std::runtime_error("Invalid or duplicate task record");
            largestId = std::max(largestId, task.id);
            candidate.push_back(std::move(task));
        }
        if (std::getline(input, line) || !input.eof() || next <= largestId)
            throw std::runtime_error("Trailing data or invalid next ID");
        validateCollection(candidate, next);
        std::size_t bytes = 0;
        for (const auto &task : candidate)
            bytes += recordBytes(task);
        fs::path openedPath = path;
        m_tasks = std::move(candidate);
        m_nextId = next;
        m_file = std::move(openedPath);
        m_dirty = false;
        m_encodedBytes = bytes;
    }

    const Task *findTask(uint64_t id) const
    {
        const auto it = std::find_if(m_tasks.begin(), m_tasks.end(), [id](const Task &task) { return task.id == id; });
        return it == m_tasks.end() ? nullptr : &*it;
    }

private:
    void validateForMutation(const Task &task) const
    {
        if (!validTask(task))
            throw std::invalid_argument("Invalid task fields or deadline");
    }

    static void validateCollection(const std::vector<Task> &tasks, uint64_t next)
    {
        if (tasks.size() > maxTasks || next == 0)
            throw std::invalid_argument("Invalid Daybook task collection");
        std::set<uint64_t> ids;
        uint64_t largestId = 0;
        std::size_t bytes = 0;
        for (const auto &task : tasks)
        {
            if (!validTask(task, true) || !ids.insert(task.id).second)
                throw std::invalid_argument("Invalid or duplicate task in Daybook");
            largestId = std::max(largestId, task.id);
            bytes += recordBytes(task);
            if (bytes > maxFileBytes - 128)
                throw std::length_error("Task document exceeds its 8 MiB storage limit");
        }
        if (next <= largestId)
            throw std::invalid_argument("Invalid next task ID");
    }

    static std::size_t recordBytes(const Task &task)
    {
        return std::to_string(task.id).size() + std::to_string(task.priority).size() +
               std::to_string(task.minutes).size() + 1 + samples::quote(task.title).size() +
               samples::quote(task.kind).size() + samples::quote(task.tags).size() +
               samples::quote(task.deadline).size() + 8;
    }

    std::vector<Task>::iterator find(uint64_t id)
    {
        return std::find_if(m_tasks.begin(), m_tasks.end(), [id](const Task &task) { return task.id == id; });
    }

    std::vector<Task> m_tasks;
    uint64_t m_nextId = 1;
    bool m_dirty = false;
    fs::path m_file;
    std::size_t m_encodedBytes = 0;
};

// Resolve a pending editor before deletion, while retaining the task the user
// actually targeted. Resolving a new draft must never select a deletion target.
template <typename Resolve> bool eraseAfterResolving(Model &model, uint64_t target, Resolve resolve)
{
    if (!target || !resolve())
        return false;
    return model.erase(target);
}
} // namespace daybook
