# KernScript Assembly Language

`module` (optional, once), `extern` declarations, `func` blocks.
Two-operand instructions separate operands with a comma.
Forward references are allowed; redefining anything is an error.

## Declarations

`module <name>` — names the script, at most once.
`extern <name> <argCount>` — declares a host function taking 0-255 arguments.
`func <name> { ... }` — script function.
`param <name>` — parameter slot, filled from pushed arguments in order.
`var <name>` — one local slot.
`vara <name> <size>` — array of size >= 1 slots.
`label <name>` — marks the next instruction as a jump target.

Literals: signed ints -2147483648..2147483647, unsigned ints
0..4294967295 (`pushu` only), 32-bit floats, `"double-quoted strings"`.
Backslashes in strings are kept verbatim (`"\n"` is two characters, not a
newline); a string must close on its line.

## Instructions

`nop` — no-op. `break` — no-op, no debugger attached. `exit` — end the script.
`movv <dst>, <src>` — copy variable to variable.
`movi <dst>, <int>` — set variable to signed constant.
`movf <dst>, <float>` — set variable to float constant.
`movs <dst>, "<str>"` — set variable to string constant.
`pushv <var>` — push variable value.
`pushi <int>` / `pushu <uint>` / `pushf <float>` / `pushs "<str>"` — push constant.
`pop` — discard top of stack; never below the frame.
`popv <var>` — store top of stack in variable and discard it.
`call <func>` — call script function; takes its parameters from the top of
the stack and leaves the return value there.
`calle <name>` — call extern function; needs exactly its declared argument
count pushed. Results the host leaves behind stay on the stack and count
toward the next call.
`ret` — return without value; ends the script in `main`.
`retv <var>` — return variable value.
`reti <int>` / `retf <float>` / `rets "<str>"` — return constant.
`add` / `sub` / `mul` / `div` — pop two numbers, push the result.
Division by zero fails the run.
`inc` / `dec` — add/subtract 1 on the top of stack, in place.
`and` / `or` / `xor` — pop two numbers, push 1 or 0. `not` — negate top
of stack in place. Zero is false, anything else is true.
`jmp <label>` — unconditional jump.
`je` / `jne` / `jg` / `jge` / `jl` / `jle <label>` — pop two values,
jump on the comparison. `je`/`jne` compare strings by value; ordered
jumps never jump on strings.

A jump to a trailing label and falling off the end return implicitly.
A runaway script is stopped after a bounded number of instructions.
Wrong argument count, empty stack and reading a never-assigned variable
fail the run.
