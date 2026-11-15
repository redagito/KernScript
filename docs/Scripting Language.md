# KernScript scripting language

Source compiles to typed assembly or directly to portable bytecode. A source
file has an optional `module` header, imports, extern declarations, and at
least one function. Execution starts at a host chosen zero argument, void
function. Every VM call runs to completion.

## Lexical rules

- Identifiers: `[A-Za-z_][A-Za-z0-9_]*` (`@` is reserved for mangled names).
- Keywords: `func extern var if else while return and or xor not module import export`.
- Integer literals: `-2147483648..2147483647` are signed;
  `2147483648..4294967295` are unsigned; larger values are errors.
- Float literals are 32-bit, such as `3.5`. Exponents are not supported.
- Strings use double quotes. Backslash sequences are kept verbatim; `"\n"`
  is two characters, not a line break. Strings end on the same line.
- `//` comments run to the end of the line. Whitespace is insignificant.

## Grammar

```ebnf
Program    := [ModuleDecl] ImportDecl* ExternDecl* FuncDecl+
ModuleDecl := "module" Ident ";"
ImportDecl := "import" Ident ";"
ExternDecl := "extern" Ident "(" [Ident ("," Ident)*] ")" "->" ReturnKind ";"
FuncDecl   := ["export"] "func" Ident "(" [Ident ("," Ident)*] ")" "->" ReturnKind Block
ReturnKind := "value" | "void"
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
Call       := [Ident "::"] Ident "(" [Expr ("," Expr)*] ")"
```

Imports follow the optional module header and precede externs and functions.
`export` applies to functions. Imports resolve through the search paths passed
to compilation; `import name;` loads `name.ks`, and that file must declare
`module name;`. Repeated imports share one loaded module. Cycles, missing files,
name mismatches, and duplicate modules are errors.

Unqualified calls resolve in the current module, including private functions.
Qualified calls resolve only to exported functions in the current module or a
direct import. Visibility is not transitive. Named module functions are
mangled as `module@function`; anonymous root functions keep flat names.

## Values and operators

Variables are dynamically typed and can hold signed or unsigned integers,
floats, or strings. Numeric zero is false and other numbers are true. Every
string is true, including the empty string. Conditions and logical operators
use these truth rules. Logical operators return integer `1` or `0`; `and` and
`or` evaluate both operands. `+ - * /` require numbers. Division by zero and
out of range results fail at runtime. Equality compares strings by value;
ordered string comparisons are false. Comparisons involving NaN are true only
for `!=`.

## Functions and externs

Return kinds are explicit and part of the function signature:

```ks
extern read() -> value;
extern emit(text) -> void;

func twice(x) -> value {
  return x * 2;
}

func main() -> void {
  emit(twice(read()));
}
```

A value function must return a value on every reachable path. A void function
may return only with `return;`. Missing returns, bare returns in value
functions, and valued returns in void functions are compile errors. A void
call cannot be used as a value. A valued call used as a statement is allowed
and its result is discarded. Extern return kinds and argument counts must
match the registered host binding. Declarations merged from imported files
must have identical signatures. Nested calls preserve earlier expression
values. Recursion is supported.

## Errors and limits

Compile errors include duplicate or undeclared names, wrong argument counts,
return-kind mismatches, reachable fallthrough in value functions, void calls
used as values, conflicting extern signatures, invalid literals, and returns
outside functions. Runtime failures include invalid operand types, division by
zero, overflow, uninitialized reads, missing host bindings, and instruction
budget exhaustion.

Blocks and expression trees are bounded at 128 levels. Flat operator chains
count toward this bound; split long chains with intermediate variables.
Import depth is also bounded at 128. Diagnostics use
`<file>:<line>:<column>: error: <message>` and may include a hint.

Return-path analysis is conservative: `if` requires both branches to return,
or a later return, and loops are assumed able to exit. Add an explicit return
after a loop in a value function, even if its condition is always true.
