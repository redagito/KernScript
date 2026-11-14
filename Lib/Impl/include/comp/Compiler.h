#pragma once

#include <kern/ICompiler.h>
#include <kern/ILogger.h>

#include "common/Program.h"

class Compiler : public kern::ICompiler
{
public:
    /** Constructs a compiler. A null logger discards diagnostics. */
    explicit Compiler(kern::ILogger *logger);

    bool compile(std::istream &sourceCode, std::ostream &asmCode) override;
    bool compile(std::istream &sourceCode, std::ostream &asmCode, const std::string &sourceName) override;
    bool compile(std::istream &sourceCode, std::ostream &asmCode, const std::string &sourceName,
                 const std::vector<std::string> &searchPaths) override;
    bool compileBytecode(std::istream &sourceCode, std::ostream &byteCode, const std::string &sourceName,
                         const std::vector<std::string> &searchPaths) override;

private:
    bool compileIr(std::istream &sourceCode, const std::string &sourceName, const std::vector<std::string> &searchPaths,
                   ir::Program &program);
    kern::ILogger *m_logger;
};
