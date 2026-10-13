# KernScript Diagnostics Guide

Compile errors print as:

```text
<file>:<line>:<col>: error: <message>
```

with a following `note: <hint>` line when a
hint applies (Parser.cpp:84-99). Assembler
lines carry no position. Runtime failures throw
`kern::RuntimeError`; the message is `what()`.

## Catalog

Cause plus fix per entry. "Ran" means produced
by `.scratch/docs_verify/driver.exe`; the rest
cite the source line.

Undeclared call:

```text
hello.ks:2:3: error: Call to undeclared function.
note: declare it with extern or func
```

Cause: the name is neither `extern` nor `func`.
Fix: declare it. Ran.

Undeclared variable:

```text
t.ks:3:8: error: Use of undeclared variable.
note: declare it with var or as a parameter
```

Cause: reading a name never declared. Fix: add
a `var` or parameter. Ran. Sibling:
"Assignment to undeclared variable."
(Lower.cpp:828).

Wrong arity:

```text
t.ks:3:3: error: Wrong argument count.
note: declared 1 parameters here
```

Cause: call count differs from declaration.
Fix: match the declared count. Ran.

Mixed returns:

```text
t.ks:6:3: error: Mixed valued and void returns.
note: use one return form in each function
```

Cause: `return 1;` and `return;` in one func.
Fix: pick one form. Ran.

Bad integer:

```text
t.ks:3:8: error: Integer literal out of range.
note: use 0 to 4294967295
```

Cause: magnitude above 2^32-1. Fix: stay in
range. Ran.

Bad float:

```text
Invalid float literal: 340282356779261762211166863146581399890.0
Parse function failed.
```

Cause: a decimal beyond 32-bit float range
reaching the assembler. Fix: use a 32-bit
float. Ran. Compiler-side sibling: "Float
literal out of range." (Parser.cpp:247,
Lower.cpp:398).

Extern misuse:

```text
t.ks:5:11: error: Inconsistent extern use.
note: use it always as a value or always as a statement
```

Cause: one extern used both as a value and as
a statement. Fix: bind valued calls to vars,
keep one form. Ran.

Stale stack value at runtime:

```text
The extern function 'hi' expects 0 arguments, but 1 were pushed.
```

Cause: a value left on the stack (e.g. `y` in
`y < hi()`) counts toward the call. Fix: hoist
extern calls into variables first. Ran.

Missing host binding:

```text
The extern function 'ghost' does not exist.
```

Cause: the script declares it but the host
never registered it. Fix: `addFunction` it.
Ran.

Failing host call: "The extern function 'f'
reported failure." Cause: host `call()` returned
false. Fix: make the host pop/set succeed.
(VirtualMachine.cpp:886-889).

"Division by zero." Cause: `x / 0` on numbers.
Fix: guard the divisor. Ran.

"Maximum execution steps exceeded." Cause:
runaway loop past the budget. Fix: band the
loop or raise it. Ran.

"Maximum runtime stack size exceeded." Cause:
frames deeper than the budget allows (0 allows
nothing). Fix: shrink frames or raise it. Ran
with a zero budget.

"Maximum call depth exceeded." Cause: nested
calls past `maxCallDepth`. Fix: recurse less
or raise it. (VirtualMachine.cpp:797).

"String length exceeds the limit." Cause:
corrupt length prefix while loading. Fix: load
bytes the assembler wrote. Ran: garbage load
throws; empty input instead returns false.

Protocol: unknown `callFunction` names return
false, never throw (ran); a failed `load`
leaves no script, so load again before calling
(IVirtualMachine.h:29-31).

Import failures: "Cannot find module 'm'",
"Import cycle detected", "Module name
mismatch", "Duplicate module", "Conflicting
extern" (Compiler.cpp:151-292). Fix: point
search paths at the files, break the cycle,
match names.

Verified: ran the driver errors mode; texts
above are byte-for-byte except entries marked
with a source line.
