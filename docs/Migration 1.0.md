# Migrating scripts and hosts to KernScript 1.0

KernScript 1.0 makes function signatures explicit and verifies the complete
program before execution. Existing source that relied on inferred returns or
extern call usage must be updated. Bytecode from earlier versions must be
rebuilt.

## Add return annotations

Every script function declares `-> value` or `-> void`:

```ks
func answer() -> value {
  return 42;
}

func main() -> void {
  print(answer());
}
```

Every reachable path in a value function must return an expression. A void
function may use bare `return;` or reach the end. The compiler no longer
infers a function's result kind from its `return` statements.

Extern declarations use the same syntax and must match their registered C++
bindings:

```ks
extern read() -> value;
extern print(text) -> void;
```

Choose the extern result kind from the actual host function return type, even
when a script discards a valued result. A valued call can now appear as a
statement; the compiler emits a discard. A void call is still invalid in a
value expression.

Imports merge externs only when argument counts and return kinds both match.

## Update custom host bindings

Replace `getArgumentCount()`, VM `popParameter`, and VM `setReturnValue` with
the new call context:

- Implement `FunctionSignature getSignature() const`.
- Implement `bool call(kern::IExternCall&)`.
- Read arguments with `getArgument(index, value)` in source order, starting
  at index 0.
- Set a return value only for a valued signature. Void bindings do not set a
  result.

Register supported C++ function pointers directly with `addFunction`; the
typed overload derives the signature automatically and accepts any number of
parameters:

```cpp
vm->addFunction("print", &print);
```

Supported argument and return types are `int`, `unsigned int`, `float`,
`double`, `bool`, and `std::string` (or references/const references for
arguments). Host argument reads perform checked conversions between supported
value types, so script values need not have an exact match for the C++ type;
out-of-range numeric conversions fail. `createExternFunction(functionPointer)`
remains available when you need to construct the binding explicitly. Script
entry calls remain zero-argument and void-returning.

## Rebuild and verify

Use `ICompiler::compileBytecode` to compile source directly to portable
bytecode, or compile to typed assembly and assemble it. Assembly and loaded
bytecode now undergo control-flow and stack verification. Fix mismatched
signatures, stack heights or heights at control-flow joins, bad return stacks,
and reachable fallthrough before execution.

The portable wire format is version 1.0 (major 1, minor 0) and is incompatible
with prior bytecode. Recompile every stored script or regenerate it from
source during upgrade. The VM rejects malformed or unsupported bytecode at
load time and clears the previously loaded program on failure.
