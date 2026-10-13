#pragma once

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "comp/Ast.h"
#include "comp/Parser.h"

/**
 * \brief Lowering and emitting free functions for Compiler::compile.
 *
 * This module holds the backend half of Compiler::compile (see
 * Compiler.h): lowerProgram lowers each AST function body to a Lowered
 * record (per-function setup with the retTemp spill slot plus the
 * statement walk), and emitProgram writes the final asm text for the
 * collected externs and lowered functions. Both are also exercised
 * directly by parser unit tests. Resolution is module scoped (see Ast.h):
 * definitions lower under their mangled name, calls emit call to the
 * resolved mangled target, and misses report the module diagnostics.
 */

struct Lowered
{
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> vars;
    std::vector<std::string> body;
};

bool lowerProgram(const ast::Program &program, kern::ILogger *logger, const std::string &sourceName,
                  std::vector<Lowered> &lowered);
void emitProgram(const ast::Program &program, const std::vector<Lowered> &lowered, std::ostream &asmCode);
