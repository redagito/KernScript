#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../../Support/Files.h"

namespace signaldesk
{
constexpr std::size_t MaxRows = 100000;
struct Row
{
    unsigned long long time{};
    double value[4]{};
};
struct ChartConfig
{
    std::string title;
    int source{};
    int window{12};
    double threshold{28};
    double hysteresis{2};
};

inline bool finiteNumber(const std::string &s, double &v)
{
    if (s.empty())
        return false;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size() && std::isfinite(v) &&
           std::abs(v) <= std::numeric_limits<float>::max();
}
inline bool integer(const std::string &s, int &v)
{
    if (s.empty())
        return false;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}
inline std::string formatNumber(double value)
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
                                      std::numeric_limits<double>::max_digits10);
    if (result.ec != std::errc{})
        throw std::runtime_error("Could not format a chart number");
    return {buffer, result.ptr};
}
inline std::vector<ChartConfig> parseLayout(const std::string &text)
{
    std::istringstream in(text);
    std::string magic, rest, trailing;
    int version{}, count{};
    if (!std::getline(in, rest))
        throw std::runtime_error("Missing layout header");
    if (!rest.empty() && rest.back() == '\r')
        rest.pop_back();
    std::istringstream header(rest);
    if (!(header >> magic >> version >> count) || (header >> trailing) || magic != "SIGNALDESK_LAYOUT" ||
        version != 1 || count < 1 || count > 6)
        throw std::runtime_error("Unsupported layout header");
    std::vector<ChartConfig> charts;
    charts.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        if (!std::getline(in, rest))
            throw std::runtime_error("Missing chart title");
        if (!rest.empty() && rest.back() == '\r')
            rest.pop_back();
        ChartConfig c;
        c.title = samples::unquote(rest);
        if (!std::getline(in, rest))
            throw std::runtime_error("Missing chart configuration");
        if (!rest.empty() && rest.back() == '\r')
            rest.pop_back();
        std::istringstream fields(rest);
        std::string trailing;
        if (!(fields >> c.source >> c.window >> c.threshold >> c.hysteresis) || (fields >> trailing) ||
            c.title.empty() || c.title.size() > 256 || c.source < 0 || c.source > 3 || c.window < 1 ||
            c.window > 10000 || !std::isfinite(c.threshold) ||
            std::abs(c.threshold) > std::numeric_limits<float>::max() || !std::isfinite(c.hysteresis) ||
            c.hysteresis > std::numeric_limits<float>::max() || c.hysteresis < 0)
            throw std::runtime_error("Invalid chart configuration");
        charts.push_back(std::move(c));
    }
    if (in >> rest)
        throw std::runtime_error("Unexpected layout content");
    return charts;
}
inline std::string serializeLayout(const std::vector<ChartConfig> &charts)
{
    if (charts.empty() || charts.size() > 6)
        throw std::runtime_error("A layout must contain one to six charts");
    std::ostringstream out;
    out << "SIGNALDESK_LAYOUT 1 " << charts.size() << "\n" << std::setprecision(17);
    for (const auto &c : charts)
    {
        if (c.title.empty() || c.title.size() > 256 || c.source < 0 || c.source > 3 || c.window < 1 ||
            c.window > 10000 || !std::isfinite(c.threshold) ||
            std::abs(c.threshold) > std::numeric_limits<float>::max() || !std::isfinite(c.hysteresis) ||
            c.hysteresis > std::numeric_limits<float>::max() || c.hysteresis < 0)
            throw std::runtime_error("Invalid chart configuration");
        out << samples::quote(c.title) << "\n"
            << c.source << ' ' << c.window << ' ' << c.threshold << ' ' << c.hysteresis << "\n";
    }
    return out.str();
}
inline void writeLayout(const std::filesystem::path &path, const std::vector<ChartConfig> &charts)
{
    const auto text = serializeLayout(charts);
    samples::writeAtomically(path, [&](std::ostream &out) { out << text; });
}
inline std::vector<ChartConfig> readLayout(const std::filesystem::path &path)
{
    return parseLayout(samples::readText(path, 1'000'000));
}

inline std::vector<Row> parseRecording(const std::string &text)
{
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line))
        throw std::runtime_error("Unexpected CSV header");
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line != "time,temperature_C,humidity_pct,power_W,fan_pct")
        throw std::runtime_error("Unexpected CSV header");
    std::vector<Row> rows;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (std::count(line.begin(), line.end(), ',') != 4)
            throw std::runtime_error("CSV row must contain exactly five fields");
        std::istringstream fields(line);
        std::string cell;
        Row row;
        if (!std::getline(fields, cell, ','))
            throw std::runtime_error("Invalid time");
        auto t = std::from_chars(cell.data(), cell.data() + cell.size(), row.time);
        if (cell.empty() || t.ec != std::errc{} || t.ptr != cell.data() + cell.size())
            throw std::runtime_error("Invalid time");
        for (double &v : row.value)
            if (!std::getline(fields, cell, ',') || !finiteNumber(cell, v))
                throw std::runtime_error("Invalid or non-finite sample value");
        if (std::getline(fields, cell, ','))
            throw std::runtime_error("Too many CSV columns");
        rows.push_back(row);
        if (rows.size() > MaxRows)
            throw std::runtime_error("A recording supports at most 100000 rows");
    }
    if (rows.empty())
        throw std::runtime_error("Recording is empty");
    return rows;
}
inline std::string serializeRecording(const std::vector<Row> &rows)
{
    if (rows.empty() || rows.size() > MaxRows)
        throw std::runtime_error("A recording needs 1 through 100000 rows");
    std::ostringstream out;
    out << "time,temperature_C,humidity_pct,power_W,fan_pct\n" << std::setprecision(17);
    for (const auto &r : rows)
    {
        out << r.time;
        for (double v : r.value)
        {
            if (!std::isfinite(v) || std::abs(v) > std::numeric_limits<float>::max())
                throw std::runtime_error("Recording value is outside the finite float32 range");
            out << ',' << v;
        }
        out << '\n';
    }
    return out.str();
}
inline void writeRecording(const std::filesystem::path &path, const std::vector<Row> &rows)
{
    const auto text = serializeRecording(rows);
    samples::writeAtomically(path, [&](std::ostream &out) { out << text; });
}
inline std::vector<Row> readRecording(const std::filesystem::path &path)
{
    return parseRecording(samples::readText(path, 20'000'000));
}
} // namespace signaldesk
