#include <catch2/catch_test_macros.hpp>

#include <clocale>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <kern/Kern.h>

#include "common/FloatLiteral.h"
#include "testutil/TestSupport.hpp"

namespace
{

class NumericLocaleGuard
{
public:
    NumericLocaleGuard()
    {
        const char *current = std::setlocale(LC_NUMERIC, nullptr);
        if (current != nullptr)
        {
            m_original = current;
        }
    }

    ~NumericLocaleGuard()
    {
        if (!m_original.empty())
        {
            std::setlocale(LC_NUMERIC, m_original.c_str());
        }
    }

    bool useGermanNumericLocale()
    {
#ifdef _WIN32
        const std::vector<const char *> candidates = {"German_Germany.1252", "de-DE", "German"};
#else
        const std::vector<const char *> candidates = {"de_DE.UTF-8", "de_DE.utf8", "de_DE", "de_DE.ISO8859-1"};
#endif
        for (const char *candidate : candidates)
        {
            if (std::setlocale(LC_NUMERIC, candidate) != nullptr)
            {
                const lconv *conventions = std::localeconv();
                if (conventions != nullptr && conventions->decimal_point != nullptr &&
                    std::string(conventions->decimal_point) == ",")
                {
                    return true;
                }
            }
        }
        return false;
    }

private:
    std::string m_original;
};

} // namespace

TEST_CASE("Compiler parses float literals independent of German numeric locale", "[integration][compiler][locale]")
{
    NumericLocaleGuard localeGuard;
    const bool available = localeGuard.useGermanNumericLocale();
#ifndef _WIN32
    if (!available)
        SKIP("German numeric locale is not installed on this host.");
#endif
    REQUIRE(available);
    REQUIRE(std::string(std::localeconv()->decimal_point) == ",");

    const std::string source =
        "func main() -> void {\n"
        "  var decimal = 12.375;\n"
        "  var tiny = 0.00000000000000000000000000000000000000000000140129846;\n"
        "  var maximum = 340282346638528859811704183484516925440.0;\n"
        "}\n";
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
    std::stringstream inputForAssembly(source);
    std::stringstream inputForBytecode(source);
    std::stringstream assembly;
    std::stringstream directBytecode;
    std::stringstream assembledBytecode;

    REQUIRE(compiler->compile(inputForAssembly, assembly));
    REQUIRE(compiler->compileBytecode(inputForBytecode, directBytecode));
    REQUIRE(testutil::assemble(assembly.str(), assembledBytecode));
    REQUIRE(directBytecode.str() == assembledBytecode.str());
}

TEST_CASE("Float literal parser rejects malformed and out of range values", "[integration][compiler][locale]")
{
    float value = 0.0f;
    REQUIRE_FALSE(parseFloatLiteral("", value));
    REQUIRE_FALSE(parseFloatLiteral("12,375", value));
    REQUIRE_FALSE(parseFloatLiteral("12.375tail", value));
    REQUIRE_FALSE(parseFloatLiteral("999999999999999999999999999999999999999999999999999999999999999999999999999999.0",
                                   value));
}

TEST_CASE("Compiler rejects out of range float literals", "[integration][compiler][locale]")
{
    const std::string source =
        "func main() -> void { var tooLarge = 999999999999999999999999999999999999999999999999999999999999999999999999999999.0; }\n";
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(nullptr));
    std::stringstream input(source);
    std::stringstream assembly;
    REQUIRE_FALSE(compiler->compile(input, assembly));
}
