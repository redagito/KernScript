#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <kern/ILogger.h>

#include "comp/Parser.h"

/**
 * \brief Abstract syntax tree for the scripting language.
 *
 * The node structure mirrors the EBNF in doc/Scripting Language.md exactly:
 * Program is an optional module header plus import, Extern and Func
 * declarations, Func carries its Block as a statement list and an export
 * flag, Stmt is the VarDecl | Assign | If | While | Return | ExprStmt
 * variant, and Expr is built through the Or > Xor > And > Cmp > Add > Mul >
 * Unary > Primary precedence chain with Call (optionally qualified as
 * module::func), literals and VarRef at the Primary level. Every node
 * carries the SrcPos span (start and end token positions) it was parsed
 * from. Trees use std::variant plus unique_ptr, no external dependencies.
 * parseProgram builds the tree without any semantic checks beyond literal
 * ranges; imports are recorded, not loaded, and qualified calls are
 * accepted but left unresolved here. Symbol, arity and classification
 * checks walk the tree later during lowering.
 */
namespace ast
{

struct Expr
{
    struct Binary
    {
        std::string op;
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
    };
    struct Unary
    {
        std::string op;
        std::unique_ptr<Expr> operand;
    };
    struct Call
    {
        std::string name;
        SrcPos namePos;
        std::vector<std::unique_ptr<Expr>> args;
        // Qualifier for module::func calls; empty when unqualified.
        // Resolution happens later: the parser only records it.
        std::string qualifier;
        SrcPos qualifierPos;
    };
    struct IntLit
    {
        std::string text;
    };
    struct UIntLit
    {
        std::string text;
    };
    struct FloatLit
    {
        std::string text;
    };
    struct StrLit
    {
        std::string text;
    };
    struct VarRef
    {
        std::string name;
    };
    using Node = std::variant<Binary, Unary, Call, IntLit, UIntLit, FloatLit, StrLit, VarRef>;
    Node node;
    SrcPos pos;
    SrcPos end;
};

struct Stmt
{
    struct VarDecl
    {
        std::string name;
        SrcPos namePos;
        std::unique_ptr<Expr> init;
    };
    struct Assign
    {
        std::string name;
        SrcPos namePos;
        std::unique_ptr<Expr> value;
    };
    struct If
    {
        std::unique_ptr<Expr> cond;
        std::vector<Stmt> thenBranch;
        std::vector<Stmt> elseBranch;
        bool hasElse = false;
    };
    struct While
    {
        std::unique_ptr<Expr> cond;
        std::vector<Stmt> body;
    };
    struct Return
    {
        std::unique_ptr<Expr> value;
    };
    struct ExprStmt
    {
        std::unique_ptr<Expr> expr;
    };
    using Node = std::variant<VarDecl, Assign, If, While, Return, ExprStmt>;
    Node node;
    SrcPos pos;
    SrcPos end;
};

struct Extern
{
    std::string name;
    std::vector<std::string> params;
    SrcPos pos;
    SrcPos end;
};

struct ModuleDecl
{
    std::string name;
    SrcPos namePos;
    SrcPos pos;
    SrcPos end;
};

struct ImportDecl
{
    std::string name;
    SrcPos namePos;
    SrcPos pos;
    SrcPos end;
};

struct Func
{
    std::string name;
    std::vector<std::string> params;
    std::vector<Stmt> body;
    SrcPos pos;
    SrcPos end;
    bool hasValuedReturn = false;
    bool hasBareReturn = false;
    // Set by the "export" prefix; an inert flag until later slices.
    bool exported = false;
};

struct Program
{
    bool hasModule = false;
    ModuleDecl module;
    // Recorded, not loaded: name resolution runs in a later slice.
    std::vector<ImportDecl> imports;
    std::vector<Extern> externs;
    std::vector<Func> funcs;
};

/**
 * \brief Parses buffered tokens into an AST, one Program per EBNF Decl*.
 * \param sourceName File name used in diagnostics (no file is read).
 * \return True on success and false on the first syntax error.
 */
bool parseProgram(const std::vector<Tok> &toks, kern::ILogger *logger, const std::string &sourceName, Program &program);

} // namespace ast
