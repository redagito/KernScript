# KernScript Host Manual

Pipeline: compile script source to assembly
text, assemble to bytecode, load it, call a
function. `Samples/HelloScript/source/main.cpp`
is the canonical flow.

## Factories and ownership

`createAssembler`, `createCompiler` and
`createVirtualMachine` return owning raw
pointers (Kern.h:21-74). Wrap them in
`unique_ptr`; the host deletes them.

Logger arguments are borrowed, never owned. A
null logger discards diagnostics; any other
logger must outlive every object using it
(ILogger.h:22-24). The no-arg factories log
through the spdlog default logger (Kern.h).

Creation fails only by throwing (plain `new`
in Lib/Api/source/Kern.cpp:25-44): `bad_alloc`,
or `kern::RuntimeError` for an absurd VM stack
budget (VirtualMachine.cpp:194-213). Ran with
`maxStackBytes` at its maximum: throws
"Maximum runtime stack size exceeds the limit."

`createExternFunction` supports 0-4 arguments;
more is a compile-time `static_assert`
(FunctionBinding.h:387-392).

## VMConfig budgets (VMConfig.h:23-29)

- `maxStackBytes` (default 1 MiB): divided by
the value size into slots, so 0 allows
nothing. Trip: "Maximum runtime stack size
exceeded." Ran with 0: the first call throws.
- `maxCallDepth` (default 1024): nested script
calls. Trip: "Maximum call depth exceeded."
- `maxStringBytes` (default 1 MiB): one string
or name read from bytecode. Trip: "String
length exceeds the limit."
- `maxTableEntries` (default 1 MiB): each
bytecode table. Trip: "String table size {}
exceeds the limit." and siblings
(VirtualMachine.cpp:578+).
- `maxExecutionSteps` (default 10M): how many
instructions one `callFunction` runs, counted
from zero on every call. Trip: "Maximum
execution steps exceeded." Ran `while (1) {}`
with 1000: throws.

## Errors: false versus throw

- `compile`/`assemble`: false on error.
- `load`: false on a bad header or version,
`kern::RuntimeError` on corrupt lengths. Empty
input returns false; 20 garbage bytes throw
"String length exceeds the limit." Ran both. A
failed load leaves no script behind: load
again before calling (IVirtualMachine.h:29-31).
- `callFunction`: false only for unknown
function names (ran: returns false, no throw).
Every script failure throws `kern::RuntimeError`
(IVirtualMachine.h:43-53).
- `IExternFunction::call` returning false
throws "The extern function '{}' reported
failure." (VirtualMachine.cpp:886-889).

## Calls run to completion

No yield or suspend API exists (no such symbols
in Lib). Each `callFunction` clears stale
stacks, runs at most `maxExecutionSteps`, then
returns or throws (VirtualMachine.cpp:379-429).
Split long work into banded calls (Cookbook).

## Threading

The VM holds no locks (no sync primitives in
Lib/Impl) and the reentrancy guard is a plain
bool. Confine each VM to one thread; never call
into one VM from two threads, and never call
back into it from inside an extern. `load`,
`clearScripts`, `callFunction`, `addFunction`
and `removeFunction` all throw "... is not
reentrant." mid-call (VirtualMachine.cpp).

## Module search paths

`compile(source, asm, name, paths)` resolves
each `import name;` to `name.ks`: exact
case-sensitive match, first hit wins
(ICompiler.h:38-48, Compiler.cpp:55-92). A
program with imports and empty paths is a
compile error. Cycles, name mismatches and
duplicates fail the compile (Compiler.cpp).

## Extern registration

`addFunction` replaces: re-registering a name
deletes the previous binding
(IVirtualMachine.h:67-69). `removeFunction`
erases; unknown names are a no-op. Empty names,
null functions and reentrant register/remove
throw, and on failure ownership stays with the
caller (IVirtualMachine.h:57-71,
VirtualMachine.cpp:431-464).

`clearScripts` drops the program but keeps host
bindings; `load` replaces the program and also
keeps them, so hot-reload needs no re-register
(ran: reload ran on old bindings, new body).

The VM never reads `getArgumentCount` (unused
in Lib/Impl): the pushed count must equal the
bytecode-declared count, so the C++ arity must
match the script declaration or Calle throws
(VirtualMachine.cpp:868-875).

## pop/set discipline

Pop in reverse push order, last argument first
(FunctionBinding.h:128-135). Each pop takes one
value above the frame; leftovers count toward
the next call (IVirtualMachine.h:86-90). `pop`
returns false only for inapplicable
conversions; empty stack, below-frame pops and
string-to-number or out-of-range conversions
throw, leaving the value in place
(IVirtualMachine.h:74-90, Value.cpp).
`setReturnValue` outside an extern call throws;
void bindings set empty, valued bindings set
exactly one value (FunctionBinding.h,
IVirtualMachine.h:109-131).

Verified: header and source lines cited above,
plus ran `.scratch/docs_verify/driver.exe` in
both modes (snippets ALL-OK, error texts match).
