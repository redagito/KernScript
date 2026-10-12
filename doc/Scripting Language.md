# KernScript Scripting Language (v1 Draft)

Source compiles to KernScript assembly. One source file holds `extern`
and `func` declarations. Execution starts at a host-chosen function;
every call runs to completion — a script cannot wait across calls.

## Lexical

- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Keywords: `func extern var if else while return and or xor not`.
- Integer literals: `-2147483648..2147483647` are signed; `2147483648..4294967295`
  are unsigned; anything larger is a compile error.
- Float literals: 32-bit, e.g. `3.5`. No exponents.
- Strings: `"double-quoted"`, kept verbatim (`"\n"` is two characters,
  not a newline); must close on the same line.
- `//` comments run to end of line. Whitespace is insignificant.

## Grammar

```ebnf
Program    := Decl*
Decl       := ExternDecl | FuncDecl
ExternDecl := "extern" Ident "(" [Ident ("," Ident)*] ")" ";"
FuncDecl   := "func" Ident "(" [Ident ("," Ident)*] ")" Block
Block      := "{" Stmt* "}"
Stmt       := VarDecl | Assign | If | While | Return | ExprStmt
VarDecl    := "var" Ident ["=" Expr] ";"
Assign     := Ident "=" Expr ";"
If         := "if" "(" Expr ")" Block ["else" Block]
While      := "while" "(" Expr ")" Block
Return     := "return" [Expr] ";"
ExprStmt   := Expr ";"
Expr       := OrExpr
OrExpr     := XorExpr ("or" XorExpr)*
XorExpr    := AndExpr ("xor" AndExpr)*
AndExpr    := CmpExpr ("and" CmpExpr)*
CmpExpr    := AddExpr (("==" | "!=" | "<" | "<=" | ">" | ">=") AddExpr)?
AddExpr    := MulExpr (("+" | "-") MulExpr)*
MulExpr    := Unary (("*" | "/") Unary)*
Unary      := ("-" | "not") Unary | Primary
Primary    := Integer | Float | String | Ident | Call | "(" Expr ")"
Call       := Ident "(" [Expr ("," Expr)*] ")"
```

No `for`, `break` or `continue` in v1.

## Types

Dynamic: a variable holds a signed int, unsigned int, float or string,
and may hold different types over time. Zero is false, anything else is
true. Truth values are ints (`1`/`0`).

## Operators

`+ - * /` need numbers; division by zero and out-of-range results fail
the run. `and`/`or`/`xor` pop both sides (no short-circuit) and push
1 or 0; `not` negates one value. `==`/`!=` compare strings by value;
ordered comparisons (`< <= > >=`) on strings never hold. Comparisons
with NaN hold only for `!=`.

## Calls

Arguments evaluate left to right. A call with the wrong number of
arguments is a compile error. A function is valued if it contains
`return expr;`, void otherwise; mixing both forms in one function is a
compile error. Using a void call as a value is a compile error; a valued
call used as a statement discards its value. Each extern must be used
consistently - either always for its value or always as a statement.
Recursion is allowed.

`extern name(params);` declares a host function; calls look like script
calls. A call needs exactly the declared argument count pushed; values
a previous extern call left behind count toward the next call, so
consume what you do not need.

## Errors

Compile errors: duplicate or undeclared names, wrong argument count,
mixed valued/void returns, void call used as a value, inconsistent extern
use, bad literals, `return` outside a function. Runtime failures (wrong
operand types, division by zero, overflow, reading a never-assigned
variable, missing host function, runaway execution past its bounded
instruction count) fail the run.
Diagnostics print as `<file>:<line>:<col>: error: <message>`, followed by a `note: <hint>` line when a hint applies.
