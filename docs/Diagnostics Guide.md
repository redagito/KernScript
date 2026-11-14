# KernScript Diagnostics Guide

Source diagnostics use `<file>:<line>:<column>: error: <message>` and may
include a `note:` hint. Imported source errors retain their original file and
location. Assembly and bytecode verification errors identify the invalid
program structure; runtime failures throw `kern::RuntimeError`.

## Common compile errors

**Undeclared function or variable.** Add the missing `extern` or `func`
declaration, or declare a local with `var`.

**Wrong argument count.** The number of call arguments must match the function
or extern signature.

**Return kind mismatch.** Declare `-> value` when returning expressions and
`-> void` when returning no value. A value function must return a value on
every reachable path; a void function may use only `return;`.

**Void call used as a value.** A void call cannot initialize a variable or be
used in an expression. A valued call can be used as a statement; its value is
discarded.

**Conflicting extern signature.** Every declaration with the same extern name
must agree on both argument count and return kind, and must match the host
binding registered with the VM.

**Nesting too deep.** Blocks and expression trees have a limit of 128 levels.
Long flat binary chains count toward the expression depth; split them into
intermediate variables.

## Assembly and bytecode verification

The assembler and bytecode loader verify symbol references, declared
signatures, stack effects, return stacks, and control-flow joins. Errors
include unknown symbols, mismatched call signatures, inconsistent stack
types/heights, malformed operands, and reachable fallthrough. Correct the
assembly or regenerate bytecode from source. Bytecode format 1.0 is not
compatible with earlier formats; rebuild older scripts.

## Runtime errors

`kern::RuntimeError` reports execution problems such as division by zero,
invalid operand types, overflow, uninitialized variable reads, missing host
bindings, max call depth, stack or string budget exhaustion, and exceeding
`maxExecutionSteps`. A host extern returning `false` also stops execution.

`callFunction` returns `false` for an unknown entry name. A failed bytecode
load returns `false` for malformed input; budget violations throw. Either
failed load clears the previously loaded program, so load a valid program
again before calling.
