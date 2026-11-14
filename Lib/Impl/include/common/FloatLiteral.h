#pragma once

#include <charconv>
#include <cmath>
#include <string>
#include <system_error>

// Parse source float literals independently of the process locale.
inline bool parseFloatLiteral(const std::string &text, float &value)
{
    float parsed = 0.0f;
    const char *const first = text.data();
    const char *const last = first + text.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != last || !std::isfinite(parsed))
    {
        return false;
    }
    value = parsed;
    return true;
}
