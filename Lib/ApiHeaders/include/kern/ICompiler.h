#pragma once

#include <istream>
#include <ostream>
#include <string>

namespace kern
{

/**
 * \brief Script compiler interface class.
 *
 * Reads and parses script source code from the input stream and writes the
 * resulting assembly source code to the output stream.
 */
class ICompiler
{
public:
    /**
     * \brief Virtual destructor for interface class.
     */
    virtual ~ICompiler() = default;

    /**
     * \brief Compiles script source code into assembly source code.
     * \return True on success and false on error.
     */
    virtual bool compile(std::istream &sourceCode, std::ostream &asmCode) = 0;

    /**
     * \brief Compiles script source code into assembly source code.
     * \param sourceName File name used in diagnostics (no file is read).
     * \return True on success and false on error.
     */
    virtual bool compile(std::istream &sourceCode, std::ostream &asmCode, const std::string &sourceName) = 0;
};
} // namespace kern