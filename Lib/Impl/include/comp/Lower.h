#pragma once

#include <string>

#include "common/Program.h"
#include "comp/Ast.h"
#include "comp/Parser.h"

bool lowerProgram(const ast::Program &program, kern::ILogger *logger, const std::string &sourceName,
                  ir::Program &lowered);
