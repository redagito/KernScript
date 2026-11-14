#pragma once

#include "../../Support/Files.h"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <deque>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace gridbook
{
constexpr int kMaxRows = 1000;
constexpr int kMaxColumns = 52;
constexpr int kMaxCells = 10000;
constexpr std::size_t kMaxText = 4096;
constexpr std::size_t kMaxFile = 8 * 1024 * 1024;

struct Address
{
    int row = 0, column = 0;
    bool operator==(const Address &) const = default;
};

inline std::string columnName(int column)
{
    std::string name;
    for (++column; column > 0; column = (column - 1) / 26)
        name.insert(name.begin(), static_cast<char>('A' + (column - 1) % 26));
    return name;
}

inline std::string name(Address address) { return columnName(address.column) + std::to_string(address.row + 1); }

inline std::optional<Address> address(const std::string &text)
{
    std::size_t i = 0;
    int column = 0;
    while (i < text.size() && ((text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= 'a' && text[i] <= 'z')))
    {
        if (column > kMaxColumns)
            return std::nullopt;
        const char c = text[i] >= 'a' ? static_cast<char>(text[i] - 'a' + 'A') : text[i];
        column = column * 26 + c - 'A' + 1;
        ++i;
    }
    if (!column || i == text.size() || text[i] == '0')
        return std::nullopt;
    int row = 0;
    const auto parsed = std::from_chars(text.data() + i, text.data() + text.size(), row);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || row < 1 || row > kMaxRows ||
        column > kMaxColumns)
        return std::nullopt;
    return Address{row - 1, column - 1};
}

struct Data
{
    int rows = 30, columns = 10;
    std::vector<std::string> cells = std::vector<std::string>(300);
    bool operator==(const Data &) const = default;
    bool contains(Address a) const { return a.row >= 0 && a.column >= 0 && a.row < rows && a.column < columns; }
    int index(Address a) const
    {
        if (!contains(a))
            throw std::out_of_range("Cell is outside the workbook: " + name(a));
        return a.row * columns + a.column;
    }
    static Data blank(int rows, int columns)
    {
        if (rows < 1 || rows > kMaxRows || columns < 1 || columns > kMaxColumns || rows * columns > kMaxCells)
            throw std::invalid_argument("Use 1 to 1000 rows, 1 to 52 columns, and at most 10000 cells");
        return {rows, columns, std::vector<std::string>(rows * columns)};
    }
    void validate() const
    {
        blank(rows, columns);
        if (cells.size() != static_cast<std::size_t>(rows * columns))
            throw std::invalid_argument("Workbook cell count does not match its dimensions");
        std::size_t total = 0;
        for (const auto &text : cells)
        {
            if (text.size() > kMaxText)
                throw std::invalid_argument("Cell text exceeds 4096 bytes");
            total += samples::quote(text).size() + 16;
            if (total > kMaxFile - 128)
                throw std::invalid_argument("Workbook text exceeds the 8 MiB file limit");
        }
    }
};

inline Data invoice()
{
    Data data;
    const std::vector<std::vector<std::string>> seed = {{"Item", "Quantity", "Unit price", "Total", "Notes"},
                                                        {"Apples", "3", "2", "=B2*C2", "Local delivery"},
                                                        {"Pears", "4", "5", "=B3*C3", ""},
                                                        {"Total", "", "", "=SUM(D2:D3)", ""},
                                                        {"Average", "", "", "=AVG(D2:D3)", ""}};
    for (std::size_t row = 0; row < seed.size(); ++row)
        for (std::size_t col = 0; col < seed[row].size(); ++col)
            data.cells[row * data.columns + col] = seed[row][col];
    return data;
}

inline Data budget()
{
    Data data;
    const std::vector<std::vector<std::string>> seed = {{"Category", "Planned", "Actual", "Remaining"},
                                                        {"Rent", "900", "900", "=B2-C2"},
                                                        {"Food", "350", "280", "=B3-C3"},
                                                        {"Transport", "120", "85", "=B4-C4"},
                                                        {"Leisure", "150", "95", "=B5-C5"},
                                                        {"Total", "=SUM(B2:B5)", "=SUM(C2:C5)", "=B6-C6"}};
    for (std::size_t row = 0; row < seed.size(); ++row)
        for (std::size_t col = 0; col < seed[row].size(); ++col)
            data.cells[row * data.columns + col] = seed[row][col];
    return data;
}

inline void saveData(const std::filesystem::path &path, const Data &data)
{
    data.validate();
    samples::writeAtomically(path,
                             [&](std::ostream &out)
                             {
                                 out << "Gridbook 1\n" << data.rows << ' ' << data.columns << '\n';
                                 for (int i = 0; i < static_cast<int>(data.cells.size()); ++i)
                                     if (!data.cells[i].empty())
                                         out << name({i / data.columns, i % data.columns}) << ' '
                                             << samples::quote(data.cells[i]) << '\n';
                             });
}

inline Data loadData(const std::filesystem::path &path)
{
    std::istringstream input(samples::readText(path, kMaxFile));
    std::string line;
    if (!std::getline(input, line) || line != "Gridbook 1")
        throw std::runtime_error("This is not a supported Gridbook workbook");
    int rows = 0, columns = 0;
    if (!std::getline(input, line))
        throw std::runtime_error("Missing workbook dimensions");
    std::istringstream dimensions(line);
    std::string extra;
    if (!(dimensions >> rows >> columns) || dimensions >> extra)
        throw std::runtime_error("Invalid workbook dimensions");
    auto data = Data::blank(rows, columns);
    std::set<int> seen;
    while (std::getline(input, line))
    {
        const auto split = line.find(' ');
        if (split == std::string::npos)
            throw std::runtime_error("Invalid workbook cell record");
        const auto cell = address(line.substr(0, split));
        if (!cell || !data.contains(*cell))
            throw std::runtime_error("Workbook cell is outside its declared dimensions");
        const int index = data.index(*cell);
        if (!seen.insert(index).second)
            throw std::runtime_error("Workbook contains a duplicate cell record");
        data.cells[index] = samples::unquote(line.substr(split + 1));
    }
    data.validate();
    return data;
}

class Workbook
{
    struct State
    {
        Data data;
        std::uint64_t revision = 0;
    };
    State m_state{invoice(), 0};
    std::uint64_t m_nextRevision = 1, m_savedRevision = 0;
    std::deque<State> m_undo, m_redo;
    void remember()
    {
        m_undo.push_back(m_state);
        m_redo.clear();
        std::size_t bytes = 0;
        for (auto it = m_undo.rbegin(); it != m_undo.rend(); ++it)
        {
            bytes += it->data.cells.size() * sizeof(std::string);
            for (const auto &text : it->data.cells)
                bytes += text.size();
        }
        while (m_undo.size() > 1 && (bytes > 16 * 1024 * 1024 || m_undo.size() > 30))
        {
            for (const auto &text : m_undo.front().data.cells)
                bytes -= text.size();
            bytes -= m_undo.front().data.cells.size() * sizeof(std::string);
            m_undo.pop_front();
        }
        m_state.revision = m_nextRevision++;
    }

public:
    const Data &data() const { return m_state.data; }
    bool dirty() const { return m_state.revision != m_savedRevision; }
    void set(Address a, std::string text)
    {
        const int index = data().index(a);
        if (text.size() > kMaxText)
            throw std::invalid_argument("Cell text exceeds 4096 bytes");
        samples::quote(text);
        if (data().cells[index] == text)
            return;
        auto candidate = data();
        candidate.cells[index] = std::move(text);
        candidate.validate();
        remember();
        m_state.data = std::move(candidate);
    }
    void resize(int rows, int columns)
    {
        auto next = Data::blank(rows, columns);
        if (rows == data().rows && columns == data().columns)
            return;
        for (int r = 0; r < data().rows; ++r)
            for (int c = 0; c < data().columns; ++c)
            {
                const auto &text = data().cells[data().index({r, c})];
                if (next.contains({r, c}))
                    next.cells[next.index({r, c})] = text;
                else if (!text.empty())
                    throw std::runtime_error("Clear cells outside the new dimensions before shrinking");
            }
        next.validate();
        remember();
        m_state.data = std::move(next);
    }
    bool undo()
    {
        if (m_undo.empty())
            return false;
        m_redo.push_back(std::move(m_state));
        m_state = std::move(m_undo.back());
        m_undo.pop_back();
        return true;
    }
    bool redo()
    {
        if (m_redo.empty())
            return false;
        m_undo.push_back(std::move(m_state));
        m_state = std::move(m_redo.back());
        m_redo.pop_back();
        return true;
    }
    void replace(Data data, bool saved)
    {
        data.validate();
        m_state = {std::move(data), m_nextRevision++};
        m_undo.clear();
        m_redo.clear();
        if (saved)
            markSaved();
    }
    void markSaved() { m_savedRevision = m_state.revision; }
    void save(const std::filesystem::path &path)
    {
        saveData(path, data());
        markSaved();
    }
    void open(const std::filesystem::path &path) { replace(loadData(path), true); }
};
} // namespace gridbook
