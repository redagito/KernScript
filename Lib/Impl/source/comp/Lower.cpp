#include "comp/Lower.h"

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "comp/Parser.h"

bool lowerProgram(const std::vector<Tok> &toks, const Program &program, kern::ILogger *logger,
                  std::vector<Lowered> &lowered) {
  lowered.clear();
  std::map<std::string, int> externUse;
  for (const auto &entry : program.funcOrder) {
    Ctx ctx;
    ctx.toks = &toks;
    ctx.pos = entry.bodyBegin;
    ctx.end = entry.bodyEnd;
    ctx.externs = &program.externs;
    ctx.funcs = &program.funcs;
    ctx.externUse = &externUse;
    ctx.logger = logger;
    ctx.funcValued = entry.hasValuedReturn;
    ctx.funcHasBare = entry.hasBareReturn;
    for (const auto &param : entry.params) {
      ctx.vars[param] = static_cast<unsigned int>(ctx.paramOrder.size());
      ctx.paramOrder.push_back(param);
    }
    if (ctx.funcValued) {
      // Hidden spill slot for 'return expr;' lowering. The name must lex
      // as an identifier (leading letter) and must not collide with a
      // parameter or it would shadow an argument slot.
      std::string candidate = "retmp";
      unsigned int suffix = 0;
      while (ctx.vars.find(candidate) != ctx.vars.end()) {
        candidate = "retmp" + std::to_string(suffix++);
      }
      ctx.retTemp = candidate;
      ctx.vars[candidate] = static_cast<unsigned int>(ctx.paramOrder.size() + ctx.varOrder.size());
      ctx.varOrder.push_back(candidate);
    }
    while (ctx.pos < ctx.end) {
      if (!parseStatement(ctx, ctx.body)) {
        return false;
      }
    }
    Lowered out;
    out.name = entry.name;
    out.params = ctx.paramOrder;
    out.vars = ctx.varOrder;
    out.body = ctx.body;
    lowered.push_back(out);
  }
  return true;
}

void emitProgram(const Program &program, const std::vector<Lowered> &lowered, std::ostream &asmCode) {
  for (const auto &entry : program.externOrder) {
    asmCode << "extern " << entry.first << " " << entry.second << "\n";
  }
  for (const auto &fn : lowered) {
    asmCode << "func " << fn.name << "\n{\n";
    for (const auto &param : fn.params) {
      asmCode << "param " << param << "\n";
    }
    for (const auto &name : fn.vars) {
      asmCode << "var " << name << "\n";
    }
    for (const auto &line : fn.body) {
      asmCode << line << "\n";
    }
    asmCode << "ret\n}\n";
  }
}
