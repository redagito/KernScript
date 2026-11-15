# KernScript assembly language

The assembler parses typed text assembly into the shared program model and
verifies signatures, control flow, and stack use before accepting it. Names are
quoted in emitted assembly so they cannot be confused with instruction words.

## Declarations

```asm
module "math"
extern "read" 0 -> value
func "math@twice" -> value {
  param "x"
  pushv "x"
  pushi 2
  mul
  ret
}
```

`module` is optional. `extern` declares a name, argument count, and return
kind (`value` or `void`). `func` declares a name and return kind. Functions
may contain `param`, `var`, and `vara` declarations; each parameter or local
name is unique within its function. Labels must be unique within a function.

Names are quoted strings. Constants use signed integers, unsigned integers,
32-bit floats, or quoted strings. String contents preserve backslashes
verbatim.

## Instructions

`nop`, `break`, and `exit` have no operands. `exit` ends execution.

`movv <dst>, <src>` copies a variable. `movi`, `movf`, and `movs` set a
variable from a signed integer, float, or string constant.

`pushv`, `pushi`, `pushu`, `pushf`, and `pushs` push a variable or constant.
`pop` discards the top value; `popv` stores it in a variable.

`call <function>` consumes the target's declared arguments and pushes one
value only for a value function. `calle <extern>` follows the declared extern
signature in the same way. `ret` returns the top value from a value function;
void functions use `ret` with an empty stack. Every return must match the
function signature.

`add`, `sub`, `mul`, and `div` pop two numbers and push the result. `inc` and
`dec` change the top value by one. `neg` negates the top numeric value; for
floats it preserves signed zero. `and`, `or`, and `xor` return integer truth
values; `not` negates the top value. `jmp` jumps unconditionally.
`je`, `jne`, `jg`, `jge`, `jl`, and `jle` pop two values and jump when the
comparison succeeds. Strings compare by value for equality; ordered string
comparisons do not succeed.

## Verification and bytecode

Assembly and loaded bytecode are checked before execution. Verification
rejects unknown symbols, invalid operand types, inconsistent stack heights or
heights at control-flow joins, call signature mismatches, bad return stacks,
unreachable malformed instructions, and functions that fall through. Loading
bad bytecode fails and clears the previously loaded program.

The portable wire format is major version 1, minor version 0. It uses fixed
width little-endian fields and explicit typed operands. The VM rejects
unsupported versions; regenerate bytecode from source if the format does not match.
