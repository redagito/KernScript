#include "../../../Samples/Gridbook/source/Workbook.h"
#include "../../../Samples/Support/Numbers.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>

TEST_CASE("Gridbook addresses support multiple columns and reject invalid references", "[unit][samples][gridbook]")
{
    using namespace gridbook;
    const Address last{999, 51};
    REQUIRE(address("AZ1000") == last);
    REQUIRE(address("aa12") == (Address{11, 26}));
    REQUIRE(name(last) == "AZ1000");
    for (const auto &invalid : {"A0", "A01", "AZ1001", "BA1", "A1junk", "1A", "A-1", "ZZZZZZZZ999999"})
        REQUIRE_FALSE(address(invalid));
}

TEST_CASE("Gridbook resizing preserves data and rejects populated cells outside the new size",
          "[unit][samples][gridbook]")
{
    using namespace gridbook;
    Workbook book;
    const auto original = book.data();
    book.resize(40, 30);
    REQUIRE(book.data().cells[book.data().index({1, 3})] == "=B2*C2");
    book.set({39, 29}, "Keep me");
    const auto expanded = book.data();
    REQUIRE_THROWS_AS(book.resize(30, 10), std::runtime_error);
    REQUIRE(book.data() == expanded);
    REQUIRE(book.undo());
    REQUIRE(book.undo());
    REQUIRE(book.data() == original);
    REQUIRE_FALSE(book.dirty());
    REQUIRE(book.redo());
    REQUIRE(book.data().rows == 40);
    REQUIRE_THROWS_AS(book.resize(1000, 52), std::invalid_argument);
    REQUIRE_THROWS_AS(book.set({0, 0}, std::string(4097, 'x')), std::invalid_argument);
}

TEST_CASE("Gridbook text serialization preserves escapes and rejects trailing or malformed values",
          "[unit][samples][gridbook]")
{
    const std::string text = "quoted \"value\", a tab\tand a newline\nwith \\ paths";
    REQUIRE(samples::unquote(samples::quote(text)) == text);
    for (const auto &invalid : {"unquoted", "\"unterminated", "\"ok\" trailing", "\"bad\\z\"", "\"bad\\\""})
        REQUIRE_THROWS_AS(samples::unquote(invalid), std::runtime_error);
    REQUIRE_THROWS_AS(samples::quote(std::string(1, '\x01')), std::invalid_argument);
}

TEST_CASE("Gridbook edits branch undo history and track the saved state", "[unit][samples][gridbook]")
{
    gridbook::Workbook book;
    book.set({1, 1}, "5");
    book.markSaved();
    book.set({1, 1}, "6");
    REQUIRE(book.dirty());
    REQUIRE(book.undo());
    REQUIRE_FALSE(book.dirty());
    book.set({1, 1}, "7");
    REQUIRE_FALSE(book.redo());
    REQUIRE(book.dirty());
    const auto current = book.data();
    auto invalid = current;
    invalid.cells.pop_back();
    REQUIRE_THROWS_AS(book.replace(std::move(invalid), true), std::invalid_argument);
    REQUIRE(book.data() == current);
}

TEST_CASE("Sample float source literals round trip without exponents", "[unit][samples][gridbook]")
{
    for (float value : {0.0f, -0.0f, 1.25f, 1e-30f, 1e30f, std::numeric_limits<float>::denorm_min(),
                        std::numeric_limits<float>::max()})
    {
        const auto text = samples::floatSource(value);
        REQUIRE(text.find_first_of("eE") == std::string::npos);
        REQUIRE(text.find('.') != std::string::npos);
        float parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, std::chars_format::fixed);
        REQUIRE(result.ec == std::errc{});
        REQUIRE(result.ptr == text.data() + text.size());
        REQUIRE(parsed == value);
        if (value == 0)
            REQUIRE(std::signbit(parsed) == std::signbit(value));
    }
    REQUIRE_THROWS_AS(samples::floatSource(std::numeric_limits<float>::infinity()), std::invalid_argument);
}
