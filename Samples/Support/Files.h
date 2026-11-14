#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>

namespace samples
{
inline void requireDataDestination(const std::filesystem::path &path)
{
    auto extension = path.extension().string();
    for (char &c : extension)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    if (extension == ".ks" && std::filesystem::exists(path))
        throw std::runtime_error("Data output cannot overwrite a KernScript source file: " + path.string());
}
inline std::string readText(const std::filesystem::path &path, std::size_t limit)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open " + path.string());
    std::string text;
    std::array<char, 4096> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount())
    {
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > limit - text.size())
            throw std::runtime_error("File exceeds the size limit: " + path.string());
        text.append(buffer.data(), count);
    }
    if (input.bad() || !input.eof())
        throw std::runtime_error("Could not finish reading " + path.string());
    return text;
}

// The original stays untouched until a complete, flushed file can replace it.
// C++23 noreplace reserves the temporary name rather than checking then opening.
inline void writeAtomically(const std::filesystem::path &destination, const std::function<void(std::ostream &)> &write)
{
    std::random_device random;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path temporary;
    std::ofstream output;
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        temporary = destination.parent_path() / (destination.filename().string() + ".pending-" + std::to_string(stamp) +
                                                 "-" + std::to_string(random()));
        output.open(temporary, std::ios::binary | std::ios::out | std::ios::noreplace);
        if (output)
            break;
        output.clear();
    }
    if (!output.is_open())
        throw std::runtime_error("Cannot create a temporary file beside " + destination.string());
    try
    {
        write(output);
        output.flush();
        if (!output)
            throw std::runtime_error("Could not finish writing " + destination.string());
        output.close();
        if (output.fail())
            throw std::runtime_error("Could not close " + destination.string());
        std::filesystem::rename(temporary, destination);
    }
    catch (...)
    {
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

inline std::filesystem::path testDirectory(const std::filesystem::path &root)
{
    std::filesystem::create_directories(root);
    std::random_device random;
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        const auto directory = root / ("run-" + std::to_string(random()) + "-" + std::to_string(random()));
        if (std::filesystem::create_directory(directory))
            return directory;
    }
    throw std::runtime_error("Cannot create a test directory under " + root.string());
}

inline std::string quote(const std::string &text)
{
    std::string result = "\"";
    for (unsigned char c : text)
    {
        switch (c)
        {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (c < 32 || c == 127)
                throw std::invalid_argument("Unsupported control character in text");
            result += static_cast<char>(c);
        }
    }
    return result + '"';
}

inline std::string unquote(const std::string &text)
{
    if (text.size() < 2 || text.front() != '"')
        throw std::runtime_error("Expected quoted text");
    std::string result;
    for (std::size_t i = 1; i < text.size(); ++i)
    {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == '"')
        {
            if (i != text.size() - 1)
                throw std::runtime_error("Unexpected text after quoted value");
            return result;
        }
        if (c == '\\')
        {
            if (++i >= text.size())
                throw std::runtime_error("Incomplete text escape");
            switch (text[i])
            {
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            default:
                throw std::runtime_error("Unknown text escape");
            }
        }
        else
        {
            if (c < 32 || c == 127)
                throw std::runtime_error("Unsupported control character in text");
            result += static_cast<char>(c);
        }
    }
    throw std::runtime_error("Unterminated quoted text");
}
} // namespace samples
