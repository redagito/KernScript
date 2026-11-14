#include "comp/Lower.h"

#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "common/Program.h"
#include "common/FloatLiteral.h"

namespace
{
struct ModuleIndex
{
    std::map<std::string, const ast::Func *> byMangled;
    std::map<std::string, std::map<std::string, const ast::Func *>> byModule;
};

struct LowerCtx
{
    const ast::Program *program = nullptr;
    const ModuleIndex *modules = nullptr;
    std::map<std::string, kern::FunctionSignature> externs;
    std::set<std::string> vars;
    std::vector<std::string> imports;
    ir::Function *function = nullptr;
    ir::Code *code = nullptr;
    std::string owner;
    std::string sourceName;
    kern::ILogger *logger = nullptr;
    unsigned labelCounter = 0;
};

SourceLocation location(const LowerCtx &ctx, SrcPos pos) { return {ctx.sourceName, pos.line, pos.col}; }

void lowerError(LowerCtx &ctx, SrcPos pos, const std::string &message, const std::string &hint = "")
{
    reportError(ctx.logger, ctx.sourceName, pos, message, hint);
}

std::string mangle(const std::string &owner, const std::string &name) { return ast::mangleName(owner, name); }

std::vector<std::string> importsOf(const ast::Program &program, const std::string &owner)
{
    const auto row = program.moduleImports.find(owner);
    if (row != program.moduleImports.end())
    {
        return row->second;
    }
    std::vector<std::string> imports;
    for (const auto &item : program.imports)
    {
        imports.push_back(item.name);
    }
    return imports;
}

bool directImport(const LowerCtx &ctx, const std::string &name)
{
    for (const auto &item : ctx.imports)
    {
        if (item == name)
        {
            return true;
        }
    }
    return false;
}

bool resolveScriptCall(LowerCtx &ctx, SrcPos qualifierPos, const std::string &qualifier, SrcPos namePos,
                       const std::string &name, const ast::Func *&target)
{
    if (qualifier.empty())
    {
        const auto found = ctx.modules->byMangled.find(mangle(ctx.owner, name));
        if (found == ctx.modules->byMangled.end())
        {
            lowerError(ctx, namePos, "Call to undeclared function.", "declare it with extern or func");
            return false;
        }
        target = found->second;
        return true;
    }
    if (qualifier == ctx.owner || directImport(ctx, qualifier))
    {
        const auto module = ctx.modules->byModule.find(qualifier);
        const ast::Func *candidate = nullptr;
        if (module != ctx.modules->byModule.end())
        {
            const auto found = module->second.find(name);
            if (found != module->second.end())
            {
                candidate = found->second;
            }
        }
        if (candidate == nullptr)
        {
            if (ctx.externs.find(name) != ctx.externs.end())
            {
                lowerError(ctx, namePos, "Extern '" + name + "' has no module.",
                           "call externs unqualified, without '::'");
            }
            else
            {
                lowerError(ctx, namePos, "Unknown function '" + name + "' in module '" + qualifier + "'.",
                           "check the function name");
            }
            return false;
        }
        if (!candidate->exported)
        {
            lowerError(ctx, namePos, qualifier + "::" + name + " is not exported.",
                       "export it in " + qualifier + ".ks or call it from inside " + qualifier);
            return false;
        }
        target = candidate;
        return true;
    }
    if (ctx.externs.find(qualifier) != ctx.externs.end())
    {
        lowerError(ctx, qualifierPos, "Extern '" + qualifier + "' has no module.",
                   "call externs unqualified, without '::'");
        return false;
    }
    lowerError(ctx, qualifierPos, "Unknown module '" + qualifier + "'.", "check the name or import it directly");
    return false;
}

bool lowerExpr(LowerCtx &ctx, const ast::Expr &expr, bool &isString);
bool lowerCall(LowerCtx &ctx, const ast::Expr::Call &call, SrcPos pos, bool asStatement)
{
    for (const auto &arg : call.args)
    {
        bool ignoredString = false;
        if (!lowerExpr(ctx, *arg, ignoredString))
        {
            return false;
        }
    }
    const auto ext = call.qualifier.empty() ? ctx.externs.find(call.name) : ctx.externs.end();
    if (ext != ctx.externs.end())
    {
        if (ext->second.argumentCount != call.args.size())
        {
            lowerError(ctx, call.namePos, "Wrong argument count.",
                       "declared " + std::to_string(ext->second.argumentCount) + " parameters here");
            return false;
        }
        ctx.code->emit(Opcode::Calle, {ir::ExternRef{call.name}}, location(ctx, pos));
        if (asStatement && ext->second.returnKind == kern::ReturnKind::Value)
        {
            ctx.code->emit(Opcode::Pop, {}, location(ctx, pos));
        }
        if (!asStatement && ext->second.returnKind == kern::ReturnKind::Void)
        {
            lowerError(ctx, call.namePos, "Void call used as a value.", "use it as a statement instead");
            return false;
        }
        return true;
    }
    const ast::Func *target = nullptr;
    if (!resolveScriptCall(ctx, call.qualifierPos, call.qualifier, call.namePos, call.name, target))
    {
        return false;
    }
    if (target->signature.argumentCount != call.args.size())
    {
        lowerError(ctx, call.namePos, "Wrong argument count.",
                   "declared " + std::to_string(target->signature.argumentCount) + " parameters here");
        return false;
    }
    if (!asStatement && target->signature.returnKind == kern::ReturnKind::Void)
    {
        lowerError(ctx, call.namePos, "Void call used as a value.", "use it as a statement instead");
        return false;
    }
    ctx.code->emit(Opcode::Call, {ir::FunctionRef{mangle(target->owner, target->name)}}, location(ctx, pos));
    if (asStatement && target->signature.returnKind == kern::ReturnKind::Value)
    {
        ctx.code->emit(Opcode::Pop, {}, location(ctx, pos));
    }
    return true;
}

bool lowerExpr(LowerCtx &ctx, const ast::Expr &expr, bool &isString)
{
    if (const auto *lit = std::get_if<ast::Expr::IntLit>(&expr.node))
    {
        char *end = nullptr;
        const long long value = std::strtoll(lit->text.c_str(), &end, 10);
        if (end == nullptr || *end != '\0' || value < 0 || value > INT32_MAX)
        {
            lowerError(ctx, expr.pos, "Invalid integer literal.");
            return false;
        }
        ctx.code->emit(Opcode::Pushi, {static_cast<int32_t>(value)}, location(ctx, expr.pos));
        isString = false;
        return true;
    }
    if (const auto *lit = std::get_if<ast::Expr::UIntLit>(&expr.node))
    {
        char *end = nullptr;
        const unsigned long long value = std::strtoull(lit->text.c_str(), &end, 10);
        if (end == nullptr || *end != '\0' || value > UINT32_MAX)
        {
            lowerError(ctx, expr.pos, "Integer literal out of range.", "use 0 to 4294967295");
            return false;
        }
        ctx.code->emit(Opcode::Pushu, {static_cast<uint32_t>(value)}, location(ctx, expr.pos));
        isString = false;
        return true;
    }
    if (const auto *lit = std::get_if<ast::Expr::FloatLit>(&expr.node))
    {
        float value = 0;
        if (!parseFloatLiteral(lit->text, value))
        {
            lowerError(ctx, expr.pos, "Float literal out of range.", "use a 32-bit float");
            return false;
        }
        ctx.code->emit(Opcode::Pushf, {value}, location(ctx, expr.pos));
        isString = false;
        return true;
    }
    if (const auto *lit = std::get_if<ast::Expr::StrLit>(&expr.node))
    {
        ctx.code->emit(Opcode::Pushs, {ir::String{lit->text}}, location(ctx, expr.pos));
        isString = true;
        return true;
    }
    if (const auto *ref = std::get_if<ast::Expr::VarRef>(&expr.node))
    {
        if (ctx.vars.find(ref->name) == ctx.vars.end())
        {
            lowerError(ctx, expr.pos, "Use of undeclared variable.", "declare it with var or as a parameter");
            return false;
        }
        ctx.code->emit(Opcode::Pushv, {ir::Local{ref->name}}, location(ctx, expr.pos));
        isString = false;
        return true;
    }
    if (const auto *call = std::get_if<ast::Expr::Call>(&expr.node))
    {
        isString = false;
        return lowerCall(ctx, *call, expr.pos, false);
    }
    if (const auto *unary = std::get_if<ast::Expr::Unary>(&expr.node))
    {
        if (unary->op == "-")
        {
            bool rhsString = false;
            if (!lowerExpr(ctx, *unary->operand, rhsString))
            {
                return false;
            }
            if (rhsString)
            {
                lowerError(ctx, expr.pos, "Arithmetic on strings.", "arithmetic needs numbers");
                return false;
            }
            ctx.code->emit(Opcode::Neg, {}, location(ctx, expr.pos));
        }
        else
        {
            bool rhsString = false;
            if (!lowerExpr(ctx, *unary->operand, rhsString))
            {
                return false;
            }
            ctx.code->emit(Opcode::Not, {}, location(ctx, expr.pos));
        }
        isString = false;
        return true;
    }
    const auto *binary = std::get_if<ast::Expr::Binary>(&expr.node);
    if (binary == nullptr)
    {
        lowerError(ctx, expr.pos, "Expected an expression.");
        return false;
    }
    bool leftString = false;
    if (!lowerExpr(ctx, *binary->left, leftString))
    {
        return false;
    }
    bool rightString = false;
    if (!lowerExpr(ctx, *binary->right, rightString))
    {
        return false;
    }
    Opcode opcode = Opcode::Nop;
    if (binary->op == "+")
        opcode = Opcode::Add;
    else if (binary->op == "-")
        opcode = Opcode::Sub;
    else if (binary->op == "*")
        opcode = Opcode::Mul;
    else if (binary->op == "/")
        opcode = Opcode::Div;
    else if (binary->op == "and")
        opcode = Opcode::And;
    else if (binary->op == "or")
        opcode = Opcode::Or;
    else if (binary->op == "xor")
        opcode = Opcode::Xor;
    else if (binary->op == "==")
        opcode = Opcode::Je;
    else if (binary->op == "!=")
        opcode = Opcode::Jne;
    else if (binary->op == "<")
        opcode = Opcode::Jl;
    else if (binary->op == "<=")
        opcode = Opcode::Jle;
    else if (binary->op == ">")
        opcode = Opcode::Jg;
    else if (binary->op == ">=")
        opcode = Opcode::Jge;
    if (binary->op == "+" || binary->op == "-" || binary->op == "*" || binary->op == "/")
    {
        if (leftString || rightString)
        {
            lowerError(ctx, expr.pos, "Arithmetic on strings.", "arithmetic needs numbers");
            return false;
        }
    }
    if (opcode == Opcode::Je || opcode == Opcode::Jne || opcode == Opcode::Jl || opcode == Opcode::Jle ||
        opcode == Opcode::Jg || opcode == Opcode::Jge)
    {
        const unsigned id = ctx.labelCounter++;
        const std::string trueLabel = "cmp_true_" + std::to_string(id);
        const std::string endLabel = "cmp_end_" + std::to_string(id);
        ctx.code->emit(opcode, {ir::LabelRef{trueLabel}}, location(ctx, expr.pos));
        ctx.code->emit(Opcode::Pushi, {int32_t{0}}, location(ctx, expr.pos));
        ctx.code->emit(Opcode::Jmp, {ir::LabelRef{endLabel}}, location(ctx, expr.pos));
        ctx.code->label(trueLabel);
        ctx.code->emit(Opcode::Pushi, {int32_t{1}}, location(ctx, expr.pos));
        ctx.code->label(endLabel);
    }
    else
    {
        ctx.code->emit(opcode, {}, location(ctx, expr.pos));
    }
    isString = false;
    return true;
}

bool lowerStmt(LowerCtx &ctx, const ast::Stmt &stmt);
bool lowerBlock(LowerCtx &ctx, const std::vector<ast::Stmt> &body)
{
    for (const auto &stmt : body)
    {
        if (!lowerStmt(ctx, stmt))
        {
            return false;
        }
    }
    return true;
}

bool lowerStmt(LowerCtx &ctx, const ast::Stmt &stmt)
{
    if (const auto *decl = std::get_if<ast::Stmt::VarDecl>(&stmt.node))
    {
        if (!ctx.vars.insert(decl->name).second)
        {
            lowerError(ctx, decl->namePos, "Variable already declared.", "use a different name");
            return false;
        }
        ctx.function->locals.push_back({decl->name, 1});
        if (decl->init != nullptr)
        {
            bool ignored = false;
            if (!lowerExpr(ctx, *decl->init, ignored))
                return false;
            ctx.code->emit(Opcode::Popv, {ir::Local{decl->name}}, location(ctx, stmt.pos));
        }
        return true;
    }
    if (const auto *assign = std::get_if<ast::Stmt::Assign>(&stmt.node))
    {
        if (ctx.vars.find(assign->name) == ctx.vars.end())
        {
            lowerError(ctx, assign->namePos, "Assignment to undeclared variable.", "declare it first with var");
            return false;
        }
        bool ignored = false;
        if (!lowerExpr(ctx, *assign->value, ignored))
            return false;
        ctx.code->emit(Opcode::Popv, {ir::Local{assign->name}}, location(ctx, stmt.pos));
        return true;
    }
    if (const auto *branch = std::get_if<ast::Stmt::If>(&stmt.node))
    {
        bool ignored = false;
        if (!lowerExpr(ctx, *branch->cond, ignored))
            return false;
        ctx.code->emit(Opcode::Not, {}, location(ctx, stmt.pos));
        ctx.code->emit(Opcode::Pushi, {int32_t{0}}, location(ctx, stmt.pos));
        const unsigned id = ctx.labelCounter++;
        const std::string elseLabel = "if_else_" + std::to_string(id);
        const std::string endLabel = "if_end_" + std::to_string(id);
        ctx.code->emit(Opcode::Jne, {ir::LabelRef{branch->hasElse ? elseLabel : endLabel}}, location(ctx, stmt.pos));
        if (!lowerBlock(ctx, branch->thenBranch))
            return false;
        if (branch->hasElse)
        {
            ctx.code->emit(Opcode::Jmp, {ir::LabelRef{endLabel}}, location(ctx, stmt.pos));
            ctx.code->label(elseLabel);
            if (!lowerBlock(ctx, branch->elseBranch))
                return false;
        }
        ctx.code->label(endLabel);
        return true;
    }
    if (const auto *loop = std::get_if<ast::Stmt::While>(&stmt.node))
    {
        const unsigned id = ctx.labelCounter++;
        const std::string start = "while_start_" + std::to_string(id);
        const std::string end = "while_end_" + std::to_string(id);
        ctx.code->label(start);
        bool ignored = false;
        if (!lowerExpr(ctx, *loop->cond, ignored))
            return false;
        ctx.code->emit(Opcode::Not, {}, location(ctx, stmt.pos));
        ctx.code->emit(Opcode::Pushi, {int32_t{0}}, location(ctx, stmt.pos));
        ctx.code->emit(Opcode::Jne, {ir::LabelRef{end}}, location(ctx, stmt.pos));
        if (!lowerBlock(ctx, loop->body))
            return false;
        ctx.code->emit(Opcode::Jmp, {ir::LabelRef{start}}, location(ctx, stmt.pos));
        ctx.code->label(end);
        return true;
    }
    if (const auto *ret = std::get_if<ast::Stmt::Return>(&stmt.node))
    {
        if (ret->value != nullptr)
        {
            bool ignored = false;
            if (!lowerExpr(ctx, *ret->value, ignored))
                return false;
        }
        ctx.code->emit(Opcode::Ret, {}, location(ctx, stmt.pos));
        return true;
    }
    if (const auto *eval = std::get_if<ast::Stmt::ExprStmt>(&stmt.node))
    {
        if (const auto *call = std::get_if<ast::Expr::Call>(&eval->expr->node))
        {
            return lowerCall(ctx, *call, eval->expr->pos, true);
        }
        bool ignored = false;
        if (!lowerExpr(ctx, *eval->expr, ignored))
            return false;
        ctx.code->emit(Opcode::Pop, {}, location(ctx, stmt.pos));
        return true;
    }
    return true;
}
} // namespace

bool lowerProgram(const ast::Program &program, kern::ILogger *logger, const std::string &sourceName,
                  ir::Program &lowered)
{
    lowered = ir::Program{};
    lowered.moduleName = program.hasModule ? program.module.name : std::string();
    std::map<std::string, kern::FunctionSignature> externs;
    for (const auto &ext : program.externs)
    {
        externs[ext.name] = ext.signature;
        lowered.externs.push_back({ext.name, ext.signature, {ext.sourceName, ext.pos.line, ext.pos.col}});
    }
    ModuleIndex modules;
    for (const auto &func : program.funcs)
    {
        modules.byMangled[mangle(func.owner, func.name)] = &func;
        modules.byModule[func.owner][func.name] = &func;
    }
    for (const auto &func : program.funcs)
    {
        LowerCtx ctx;
        ctx.program = &program;
        ctx.modules = &modules;
        ctx.externs = externs;
        ctx.imports = importsOf(program, func.owner);
        ctx.owner = func.owner;
        ctx.sourceName = func.sourceName.empty() ? sourceName : func.sourceName;
        ctx.logger = logger;
        ir::Function loweredFunc;
        loweredFunc.name = mangle(func.owner, func.name);
        loweredFunc.signature = func.signature;
        loweredFunc.params = func.params;
        loweredFunc.location = {ctx.sourceName, func.pos.line, func.pos.col};
        ctx.function = &loweredFunc;
        ctx.code = &loweredFunc.code;
        for (const auto &param : func.params)
            ctx.vars.insert(param);
        if (!lowerBlock(ctx, func.body))
            return false;
        if (func.signature.returnKind == kern::ReturnKind::Void)
        {
            loweredFunc.code.emit(Opcode::Ret, {}, {ctx.sourceName, func.end.line, func.end.col});
        }
        lowered.functions.push_back(std::move(loweredFunc));
    }
    return true;
}
