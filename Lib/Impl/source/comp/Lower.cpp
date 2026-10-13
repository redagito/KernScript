#include "comp/Lower.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "comp/Ast.h"
#include "comp/Parser.h"

namespace
{

// Mutable per-function lowering state. It mirrors the old token-walking
// state one for one (symbol tables, emitted body lines, label counter,
// logger) so the tree walk emits identical assembly.
struct LowerCtx
{
    const std::map<std::string, unsigned> *externArity = nullptr;
    const std::map<std::string, const ast::Func *> *funcs = nullptr;
    std::map<std::string, int> *externUse = nullptr; // 1 = value, 2 = statement
    std::map<std::string, unsigned> vars;
    std::vector<std::string> paramOrder;
    std::vector<std::string> varOrder;
    std::string retTemp;
    bool funcValued = false;
    bool funcHasBare = false;
    unsigned labelCounter = 0;
    kern::ILogger *logger = nullptr;
    std::string sourceName;
};

std::string arityHint(unsigned expected) { return "declared " + std::to_string(expected) + " parameters here"; }

void lowerError(LowerCtx &ctx, SrcPos pos, const std::string &message, const std::string &hint = "")
{
    reportError(ctx.logger, ctx.sourceName, pos, message, hint);
}

bool markExternUse(LowerCtx &ctx, SrcPos pos, const std::string &name, bool valueUse)
{
    const int want = valueUse ? 1 : 2;
    auto it = ctx.externUse->find(name);
    if (it == ctx.externUse->end())
    {
        (*ctx.externUse)[name] = want;
        return true;
    }
    if (it->second != want)
    {
        lowerError(ctx, pos, "Inconsistent extern use.", "use it always as a value or always as a statement");
        return false;
    }
    return true;
}

// Emits one integer push for an unsigned magnitude (no sign). Rejects
// magnitudes above 4294967295. Uses pushi up to 2147483647, pushu above.
bool emitPushUint(LowerCtx &ctx, SrcPos pos, const std::string &digits, std::vector<std::string> &out)
{
    unsigned long long value = 0;
    for (char c : digits)
    {
        if (c < '0' || c > '9')
        {
            lowerError(ctx, pos, "Invalid integer literal.");
            return false;
        }
        value = value * 10 + static_cast<unsigned long long>(c - '0');
        if (value > 4294967295ULL)
        {
            lowerError(ctx, pos, "Integer literal out of range.", "use 0 to 4294967295");
            return false;
        }
    }
    if (value <= 2147483647ULL)
    {
        out.push_back("pushi " + digits);
    }
    else
    {
        out.push_back("pushu " + digits);
    }
    return true;
}

bool lowerExpr(LowerCtx &ctx, const ast::Expr &expr, std::vector<std::string> &out, bool &isString);

// Lowers a call used as a value. Extern calls mark value-use; script calls
// require a valued callee and leave the return value on the stack.
bool lowerValueCall(LowerCtx &ctx, SrcPos namePos, const std::string &name,
                    const std::vector<std::unique_ptr<ast::Expr>> &args, std::vector<std::string> &out)
{
    std::vector<std::string> lowered;
    for (const auto &arg : args)
    {
        bool argIsString = false;
        if (!lowerExpr(ctx, *arg, lowered, argIsString))
        {
            return false;
        }
    }
    const unsigned count = static_cast<unsigned>(args.size());
    const auto ext = ctx.externArity->find(name);
    if (ext != ctx.externArity->end())
    {
        if (count != ext->second)
        {
            lowerError(ctx, namePos, "Wrong argument count.", arityHint(ext->second));
            return false;
        }
        if (!markExternUse(ctx, namePos, name, true))
        {
            return false;
        }
        out.insert(out.end(), lowered.begin(), lowered.end());
        out.push_back("calle " + name);
        return true;
    }
    const auto fn = ctx.funcs->find(name);
    if (fn != ctx.funcs->end())
    {
        const unsigned expected = static_cast<unsigned>(fn->second->params.size());
        if (count != expected)
        {
            lowerError(ctx, namePos, "Wrong argument count.", arityHint(expected));
            return false;
        }
        if (!fn->second->hasValuedReturn)
        {
            lowerError(ctx, namePos, "Void call used as a value.", "use it as a statement instead");
            return false;
        }
        out.insert(out.end(), lowered.begin(), lowered.end());
        out.push_back("call " + name);
        return true;
    }
    lowerError(ctx, namePos, "Call to undeclared function.", "declare it with extern or func");
    return false;
}

// Lowers a call used as a statement. Valued script calls discard the return
// value with pop; extern calls mark statement-use and emit a bare calle.
bool lowerCallStmt(LowerCtx &ctx, SrcPos namePos, const std::string &name,
                   const std::vector<std::unique_ptr<ast::Expr>> &args, std::vector<std::string> &out)
{
    std::vector<std::string> lowered;
    for (const auto &arg : args)
    {
        bool argIsString = false;
        if (!lowerExpr(ctx, *arg, lowered, argIsString))
        {
            return false;
        }
    }
    const unsigned count = static_cast<unsigned>(args.size());
    const auto ext = ctx.externArity->find(name);
    if (ext != ctx.externArity->end())
    {
        if (count != ext->second)
        {
            lowerError(ctx, namePos, "Wrong argument count.", arityHint(ext->second));
            return false;
        }
        if (!markExternUse(ctx, namePos, name, false))
        {
            return false;
        }
        out.insert(out.end(), lowered.begin(), lowered.end());
        out.push_back("calle " + name);
        return true;
    }
    const auto fn = ctx.funcs->find(name);
    if (fn != ctx.funcs->end())
    {
        const unsigned expected = static_cast<unsigned>(fn->second->params.size());
        if (count != expected)
        {
            lowerError(ctx, namePos, "Wrong argument count.", arityHint(expected));
            return false;
        }
        out.insert(out.end(), lowered.begin(), lowered.end());
        out.push_back("call " + name);
        if (fn->second->hasValuedReturn)
        {
            out.push_back("pop");
        }
        return true;
    }
    lowerError(ctx, namePos, "Call to undeclared function.", "declare it with extern or func");
    return false;
}

// Lowers one optional comparison to a 1/0 value. Plain arithmetic passes
// through; with a comparison operator the left/right pushes are followed by
// the true-jump over pushi 0 / pushi 1 so the stack holds exactly one
// numeric value for an enclosing logic operator or condition test.
bool lowerCompare(LowerCtx &ctx, const std::string &op, const ast::Expr &left, const ast::Expr &right,
                  std::vector<std::string> &out)
{
    std::vector<std::string> leftOut;
    bool leftIsString = false;
    if (!lowerExpr(ctx, left, leftOut, leftIsString))
    {
        return false;
    }
    std::vector<std::string> rightOut;
    bool rightIsString = false;
    if (!lowerExpr(ctx, right, rightOut, rightIsString))
    {
        return false;
    }
    std::string trueJump;
    if (op == "==")
    {
        trueJump = "je";
    }
    else if (op == "!=")
    {
        trueJump = "jne";
    }
    else if (op == "<")
    {
        trueJump = "jl";
    }
    else if (op == "<=")
    {
        trueJump = "jle";
    }
    else if (op == ">")
    {
        trueJump = "jg";
    }
    else
    {
        trueJump = "jge";
    }
    out.insert(out.end(), leftOut.begin(), leftOut.end());
    out.insert(out.end(), rightOut.begin(), rightOut.end());
    const unsigned id = ctx.labelCounter++;
    const std::string trueLabel = "cmp_true_" + std::to_string(id);
    const std::string endLabel = "cmp_end_" + std::to_string(id);
    out.push_back(trueJump + " " + trueLabel);
    out.push_back("pushi 0");
    out.push_back("jmp " + endLabel);
    out.push_back("label " + trueLabel);
    out.push_back("pushi 1");
    out.push_back("label " + endLabel);
    return true;
}

bool lowerExpr(LowerCtx &ctx, const ast::Expr &expr, std::vector<std::string> &out, bool &isString)
{
    if (const auto *lit = std::get_if<ast::Expr::IntLit>(&expr.node))
    {
        isString = false;
        return emitPushUint(ctx, expr.pos, lit->text, out);
    }
    if (const auto *lit = std::get_if<ast::Expr::UIntLit>(&expr.node))
    {
        isString = false;
        return emitPushUint(ctx, expr.pos, lit->text, out);
    }
    if (const auto *lit = std::get_if<ast::Expr::FloatLit>(&expr.node))
    {
        isString = false;
        errno = 0;
        char *end = nullptr;
        const float value = std::strtof(lit->text.c_str(), &end);
        if (end == nullptr || *end != '\0' || !std::isfinite(value))
        {
            lowerError(ctx, expr.pos, "Float literal out of range.", "use a 32-bit float");
            return false;
        }
        out.push_back("pushf " + lit->text);
        return true;
    }
    if (const auto *lit = std::get_if<ast::Expr::StrLit>(&expr.node))
    {
        out.push_back("pushs \"" + lit->text + "\"");
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
        out.push_back("pushv " + ref->name);
        isString = false;
        return true;
    }
    if (const auto *call = std::get_if<ast::Expr::Call>(&expr.node))
    {
        isString = false;
        return lowerValueCall(ctx, call->namePos, call->name, call->args, out);
    }
    if (const auto *unary = std::get_if<ast::Expr::Unary>(&expr.node))
    {
        if (unary->op == "-")
        {
            std::vector<std::string> rhs;
            bool rhsIsString = false;
            if (!lowerExpr(ctx, *unary->operand, rhs, rhsIsString))
            {
                return false;
            }
            if (rhsIsString)
            {
                lowerError(ctx, expr.pos, "Arithmetic on strings.", "arithmetic needs numbers");
                return false;
            }
            // Unary minus as 0 - x: push 0, push x, sub = 0 - x.
            out.push_back("pushi 0");
            out.insert(out.end(), rhs.begin(), rhs.end());
            out.push_back("sub");
            isString = false;
            return true;
        }
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!lowerExpr(ctx, *unary->operand, rhs, rhsIsString))
        {
            return false;
        }
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back("not");
        isString = false;
        return true;
    }
    const auto *binary = std::get_if<ast::Expr::Binary>(&expr.node);
    if (binary == nullptr)
    {
        lowerError(ctx, expr.pos, "Expected an expression.");
        return false;
    }
    const std::string &op = binary->op;
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=")
    {
        isString = false;
        return lowerCompare(ctx, op, *binary->left, *binary->right, out);
    }
    std::vector<std::string> leftOut;
    bool leftIsString = false;
    if (!lowerExpr(ctx, *binary->left, leftOut, leftIsString))
    {
        return false;
    }
    std::vector<std::string> rightOut;
    bool rightIsString = false;
    if (!lowerExpr(ctx, *binary->right, rightOut, rightIsString))
    {
        return false;
    }
    if (op == "+" || op == "-" || op == "*" || op == "/")
    {
        if (leftIsString || rightIsString)
        {
            lowerError(ctx, expr.pos, "Arithmetic on strings.", "arithmetic needs numbers");
            return false;
        }
        // Stack discipline: left already pushed, push right, then combine as
        // left OP right (sub/div are not commutative).
        out.insert(out.end(), leftOut.begin(), leftOut.end());
        out.insert(out.end(), rightOut.begin(), rightOut.end());
        if (op == "+")
        {
            out.push_back("add");
        }
        else if (op == "-")
        {
            out.push_back("sub");
        }
        else if (op == "*")
        {
            out.push_back("mul");
        }
        else
        {
            out.push_back("div");
        }
        isString = false;
        return true;
    }
    out.insert(out.end(), leftOut.begin(), leftOut.end());
    out.insert(out.end(), rightOut.begin(), rightOut.end());
    out.push_back(op);
    isString = false;
    return true;
}

// Lowers a full value condition for if/while. The expression always leaves
// exactly one numeric value (comparisons lower to 1/0 above); pushing 0 and
// je-ing to the else/end label jumps when the value is false, so the
// then-block runs on fallthrough.
bool lowerCondition(LowerCtx &ctx, const ast::Expr &cond, std::vector<std::string> &out, std::string &falseJump)
{
    bool valueIsString = false;
    if (!lowerExpr(ctx, cond, out, valueIsString))
    {
        return false;
    }
    out.push_back("pushi 0");
    falseJump = "je";
    return true;
}

bool lowerStmt(LowerCtx &ctx, const ast::Stmt &stmt, std::vector<std::string> &out);

// Splits "mnemonic operand..." into its two parts; operand keeps everything
// after the first space verbatim (string literals may contain spaces).
bool splitLine(const std::string &line, std::string &mnemonic, std::string &operand)
{
    const size_t space = line.find(' ');
    if (space == std::string::npos)
    {
        mnemonic = line;
        operand.clear();
        return true;
    }
    mnemonic = line.substr(0, space);
    operand = line.substr(space + 1);
    return true;
}

// Folds adjacent push/pop pairs in a finished function body. Each rule is
// stack-neutral (push+pop nets zero, mov touches nothing) and value-identical,
// so valid programs behave exactly as before:
//   pushv x | pushi n | pushf f | pushs s  +  popv v  ->  movv|movi|movf|movs v, ...
//   pushu n + popv v -> movi v, n only when n fits int32, else kept
//   push... + popv t + retv t  ->  reti|retf|rets|retv (return spill bypass)
//   push... + pop  ->  dropped (value computed and discarded)
// Labels are lines too, so control flow always breaks a pair. Pops below the
// frame or of uninitialized values still throw in the unfolded paths; folded
// movs carry the same range checks through the assembler.
void peephole(std::vector<std::string> &body)
{
    std::vector<std::string> folded;
    folded.reserve(body.size());
    size_t i = 0;
    while (i < body.size())
    {
        std::string pushOp;
        std::string pushArg;
        std::string popVar;
        std::string retVar;
        std::string retDummy;
        const bool isPush =
            splitLine(body[i], pushOp, pushArg) &&
            (pushOp == "pushv" || pushOp == "pushi" || pushOp == "pushu" || pushOp == "pushf" || pushOp == "pushs");
        const bool hasNext = i + 1 < body.size();
        std::string nextOp;
        std::string nextArg;
        if (hasNext)
        {
            splitLine(body[i + 1], nextOp, nextArg);
        }
        const bool isPopv = hasNext && nextOp == "popv" && !nextArg.empty() && nextArg.find(' ') == std::string::npos;
        if (isPush && isPopv)
        {
            popVar = nextArg;
        }
        const bool hasThird = i + 2 < body.size();
        if (hasThird)
        {
            splitLine(body[i + 2], retVar, retDummy);
        }
        const bool isRetvSame =
            hasThird && retVar == "retv" && retDummy == popVar && !popVar.empty() && isPush && isPopv;
        if (isRetvSame)
        {
            // Return spill bypass: the pushed value is the return value.
            if (pushOp == "pushv")
            {
                folded.push_back("retv " + pushArg);
            }
            else if (pushOp == "pushi")
            {
                folded.push_back("reti " + pushArg);
            }
            else if (pushOp == "pushf")
            {
                folded.push_back("retf " + pushArg);
            }
            else if (pushOp == "pushs")
            {
                folded.push_back("rets " + pushArg);
            }
            else if (pushOp == "pushu")
            {
                const unsigned long long value = std::stoull(pushArg);
                if (value <= 2147483647ULL)
                {
                    folded.push_back("reti " + pushArg);
                }
                else
                {
                    folded.push_back(body[i]);
                    folded.push_back(body[i + 1]);
                    folded.push_back(body[i + 2]);
                }
            }
            i += 3;
            continue;
        }
        if (isPush && isPopv)
        {
            if (pushOp == "pushv")
            {
                folded.push_back("movv " + popVar + ", " + pushArg);
            }
            else if (pushOp == "pushi")
            {
                folded.push_back("movi " + popVar + ", " + pushArg);
            }
            else if (pushOp == "pushf")
            {
                folded.push_back("movf " + popVar + ", " + pushArg);
            }
            else if (pushOp == "pushs")
            {
                folded.push_back("movs " + popVar + ", " + pushArg);
            }
            else if (pushOp == "pushu")
            {
                const unsigned long long value = std::stoull(pushArg);
                if (value <= 2147483647ULL)
                {
                    folded.push_back("movi " + popVar + ", " + pushArg);
                }
                else
                {
                    folded.push_back(body[i]);
                    folded.push_back(body[i + 1]);
                }
            }
            i += 2;
            continue;
        }
        if (isPush && hasNext && nextOp == "pop" && nextArg.empty())
        {
            // Computed and discarded: drop both lines.
            i += 2;
            continue;
        }
        folded.push_back(body[i]);
        ++i;
    }
    body = folded;
}
bool lowerIf(LowerCtx &ctx, const ast::Stmt::If &node, std::vector<std::string> &out)
{
    std::vector<std::string> cond;
    std::string falseJump;
    if (!lowerCondition(ctx, *node.cond, cond, falseJump))
    {
        return false;
    }
    std::vector<std::string> thenBody;
    for (const auto &stmt : node.thenBranch)
    {
        if (!lowerStmt(ctx, stmt, thenBody))
        {
            return false;
        }
    }
    std::vector<std::string> elseBody;
    if (node.hasElse)
    {
        for (const auto &stmt : node.elseBranch)
        {
            if (!lowerStmt(ctx, stmt, elseBody))
            {
                return false;
            }
        }
    }
    // Unique labels per if so nested and sequential ifs cannot collide.
    const unsigned id = ctx.labelCounter++;
    const std::string elseLabel = "if_else_" + std::to_string(id);
    const std::string endLabel = "if_end_" + std::to_string(id);
    out.insert(out.end(), cond.begin(), cond.end());
    out.push_back(falseJump + " " + (node.hasElse ? elseLabel : endLabel));
    out.insert(out.end(), thenBody.begin(), thenBody.end());
    if (node.hasElse)
    {
        out.push_back("jmp " + endLabel);
        out.push_back("label " + elseLabel);
        out.insert(out.end(), elseBody.begin(), elseBody.end());
        out.push_back("label " + endLabel);
    }
    else
    {
        out.push_back("label " + endLabel);
    }
    return true;
}

bool lowerWhile(LowerCtx &ctx, const ast::Stmt::While &node, std::vector<std::string> &out)
{
    std::vector<std::string> cond;
    std::string falseJump;
    if (!lowerCondition(ctx, *node.cond, cond, falseJump))
    {
        return false;
    }
    std::vector<std::string> loopBody;
    for (const auto &stmt : node.body)
    {
        if (!lowerStmt(ctx, stmt, loopBody))
        {
            return false;
        }
    }
    // Unique labels per loop so sequential and nested whiles cannot collide.
    const unsigned id = ctx.labelCounter++;
    const std::string startLabel = "while_start_" + std::to_string(id);
    const std::string endLabel = "while_end_" + std::to_string(id);
    out.push_back("label " + startLabel);
    out.insert(out.end(), cond.begin(), cond.end());
    out.push_back(falseJump + " " + endLabel);
    out.insert(out.end(), loopBody.begin(), loopBody.end());
    out.push_back("jmp " + startLabel);
    out.push_back("label " + endLabel);
    return true;
}

// Lowers 'return [Expr]'. A valued return evaluates the expression above
// the frame, spills it to the hidden return slot and returns it with retv;
// a bare return emits ret. Mixing both forms in one function is rejected.
bool lowerReturn(LowerCtx &ctx, SrcPos pos, const ast::Stmt::Return &node, std::vector<std::string> &out)
{
    if (node.value == nullptr)
    {
        if (ctx.funcValued)
        {
            lowerError(ctx, pos, "Mixed valued and void returns.", "use one return form in each function");
            return false;
        }
        out.push_back("ret");
        return true;
    }
    if (ctx.funcHasBare || !ctx.funcValued)
    {
        lowerError(ctx, pos, "Mixed valued and void returns.", "use one return form in each function");
        return false;
    }
    std::vector<std::string> value;
    bool valueIsString = false;
    if (!lowerExpr(ctx, *node.value, value, valueIsString))
    {
        return false;
    }
    out.insert(out.end(), value.begin(), value.end());
    out.push_back("popv " + ctx.retTemp);
    out.push_back("retv " + ctx.retTemp);
    return true;
}

bool lowerStmt(LowerCtx &ctx, const ast::Stmt &stmt, std::vector<std::string> &out)
{
    if (const auto *node = std::get_if<ast::Stmt::If>(&stmt.node))
    {
        return lowerIf(ctx, *node, out);
    }
    if (const auto *node = std::get_if<ast::Stmt::While>(&stmt.node))
    {
        return lowerWhile(ctx, *node, out);
    }
    if (const auto *node = std::get_if<ast::Stmt::Return>(&stmt.node))
    {
        return lowerReturn(ctx, stmt.pos, *node, out);
    }
    if (const auto *node = std::get_if<ast::Stmt::VarDecl>(&stmt.node))
    {
        if (ctx.vars.find(node->name) != ctx.vars.end())
        {
            lowerError(ctx, node->namePos, "Duplicate variable name.", "use a different name");
            return false;
        }
        ctx.vars[node->name] = static_cast<unsigned>(ctx.paramOrder.size() + ctx.varOrder.size());
        ctx.varOrder.push_back(node->name);
        if (node->init != nullptr)
        {
            std::vector<std::string> init;
            bool initIsString = false;
            if (!lowerExpr(ctx, *node->init, init, initIsString))
            {
                return false;
            }
            out.insert(out.end(), init.begin(), init.end());
            out.push_back("popv " + node->name);
        }
        return true;
    }
    if (const auto *node = std::get_if<ast::Stmt::Assign>(&stmt.node))
    {
        if (ctx.vars.find(node->name) == ctx.vars.end())
        {
            lowerError(ctx, node->namePos, "Assignment to undeclared variable.",
                       "declare it with var or as a parameter");
            return false;
        }
        std::vector<std::string> rhs;
        bool rhsIsString = false;
        if (!lowerExpr(ctx, *node->value, rhs, rhsIsString))
        {
            return false;
        }
        out.insert(out.end(), rhs.begin(), rhs.end());
        out.push_back("popv " + node->name);
        return true;
    }
    const auto *node = std::get_if<ast::Stmt::ExprStmt>(&stmt.node);
    if (node == nullptr)
    {
        lowerError(ctx, stmt.pos, "Expected a statement.");
        return false;
    }
    // A call used as a statement keeps extern statement-use; anything else
    // is a general expression statement whose value is discarded.
    if (const auto *call = std::get_if<ast::Expr::Call>(&node->expr->node))
    {
        return lowerCallStmt(ctx, call->namePos, call->name, call->args, out);
    }
    std::vector<std::string> value;
    bool valueIsString = false;
    if (!lowerExpr(ctx, *node->expr, value, valueIsString))
    {
        return false;
    }
    out.insert(out.end(), value.begin(), value.end());
    out.push_back("pop");
    return true;
}

} // namespace

bool lowerProgram(const ast::Program &program, kern::ILogger *logger, const std::string &sourceName,
                  std::vector<Lowered> &lowered)
{
    lowered.clear();
    std::map<std::string, unsigned> externArity;
    for (const auto &ext : program.externs)
    {
        externArity[ext.name] = static_cast<unsigned>(ext.params.size());
    }
    std::map<std::string, const ast::Func *> funcs;
    for (const auto &func : program.funcs)
    {
        funcs[func.name] = &func;
    }
    std::map<std::string, int> externUse;
    for (const auto &func : program.funcs)
    {
        LowerCtx ctx;
        ctx.externArity = &externArity;
        ctx.funcs = &funcs;
        ctx.externUse = &externUse;
        ctx.logger = logger;
        ctx.sourceName = sourceName;
        ctx.funcValued = func.hasValuedReturn;
        ctx.funcHasBare = func.hasBareReturn;
        for (const auto &param : func.params)
        {
            ctx.vars[param] = static_cast<unsigned>(ctx.paramOrder.size());
            ctx.paramOrder.push_back(param);
        }
        if (ctx.funcValued)
        {
            // Hidden spill slot for 'return expr;' lowering. The name must lex
            // as an identifier (leading letter) and must not collide with a
            // parameter or it would shadow an argument slot.
            std::string candidate = "retmp";
            unsigned suffix = 0;
            while (ctx.vars.find(candidate) != ctx.vars.end())
            {
                candidate = "retmp" + std::to_string(suffix++);
            }
            ctx.retTemp = candidate;
            ctx.vars[candidate] = static_cast<unsigned>(ctx.paramOrder.size() + ctx.varOrder.size());
            ctx.varOrder.push_back(candidate);
        }
        std::vector<std::string> body;
        for (const auto &stmt : func.body)
        {
            if (!lowerStmt(ctx, stmt, body))
            {
                return false;
            }
        }
        // Adjacent push/pop pairs are always redundant (stack-neutral,
        // value-identical); fold them after the whole body is visible so
        // pairs across statement boundaries collapse too.
        peephole(body);
        if (ctx.funcValued)
        {
            // A bypassed return spill leaves the hidden slot dead: the only
            // lines that can reference it are its own popv/retv pair, matched
            // whole so user names merely containing it never count.
            const std::string popLine = "popv " + ctx.retTemp;
            const std::string retLine = "retv " + ctx.retTemp;
            bool spillUsed = false;
            for (const std::string &line : body)
            {
                if (line == popLine || line == retLine)
                {
                    spillUsed = true;
                    break;
                }
            }
            if (!spillUsed)
            {
                ctx.vars.erase(ctx.retTemp);
                ctx.varOrder.erase(std::remove(ctx.varOrder.begin(), ctx.varOrder.end(), ctx.retTemp),
                                   ctx.varOrder.end());
            }
        }
        Lowered out;
        out.name = func.name;
        out.params = ctx.paramOrder;
        out.vars = ctx.varOrder;
        out.body = body;
        lowered.push_back(out);
    }
    return true;
}

void emitProgram(const ast::Program &program, const std::vector<Lowered> &lowered, std::ostream &asmCode)
{
    for (const auto &ext : program.externs)
    {
        asmCode << "extern " << ext.name << " " << ext.params.size() << "\n";
    }
    for (const auto &fn : lowered)
    {
        asmCode << "func " << fn.name << "\n{\n";
        for (const auto &param : fn.params)
        {
            asmCode << "param " << param << "\n";
        }
        for (const auto &name : fn.vars)
        {
            asmCode << "var " << name << "\n";
        }
        for (const auto &line : fn.body)
        {
            asmCode << line << "\n";
        }
        asmCode << "ret\n}\n";
    }
}
