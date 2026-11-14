#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace samples
{
// The source language uses decimal float literals without exponent notation.
// Convert a finite host value to the shortest fixed-point spelling that still
// round-trips through float32, including subnormals and signed zero.
inline std::string floatSource(float value)
{
    if (!std::isfinite(value))
        throw std::invalid_argument("A formula number must be finite");
    std::array<char, 128> buffer{};
    const auto formatted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed);
    if (formatted.ec != std::errc{})
        throw std::runtime_error("Could not format formula number");
    std::string result(buffer.data(), formatted.ptr);
    if (result.find('.') == std::string::npos)
        result += ".0";
    return result;
}
} // namespace samples
