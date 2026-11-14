# KernScript Host Manual

Compile source to verified bytecode with `ICompiler::compileBytecode`, or
compile source to typed assembly and assemble it. Load the bytecode and call a
zero argument, void entry function. `Samples/HelloKern/source/main.cpp`
shows the basic host flow.

## Factories and ownership

`createAssembler`, `createCompiler`, and `createVirtualMachine` return owning
raw pointers. Wrap them in `std::unique_ptr`. Loggers are borrowed and must
outlive objects that use them. A null logger discards diagnostics.

`addFunction` takes ownership of a binding on success. Registering a name
again replaces the previous binding; `removeFunction` deletes it. Empty names,
null bindings, and registry changes during execution are rejected. Referenced
host objects must outlive their bindings.

Register a normal C++ function pointer directly. The typed `addFunction`
overload supports any parameter count when each argument and return type is
one of `int`, `unsigned int`, `float`, `double`, `bool`, or `std::string`
(arguments may be const or references to these types):

```cpp
int mix(int left, unsigned int right, bool enabled, const std::string &label);
vm->addFunction("mix", &mix);
```

The wrapper derives the script argument count and return kind from the C++
signature. Argument reads convert numeric values with range checks, and can
convert numbers to strings or numeric values to `bool`; they do not require
the script value to have the exact C++ type. Strings cannot convert to numeric
types or `bool`. Out-of-range numeric conversions fail with
`kern::RuntimeError`.

`createExternFunction(functionPointer)` remains available when you want to
construct the wrapper explicitly. It supports the same types and arbitrary
parameter counts, and exposes the signature through `getSignature()`.

## Custom extern bindings

Implement `kern::IExternFunction::getSignature() const` and
`call(kern::IExternCall&)`. Arguments are read by zero-based index in source
order, and a valued binding sets exactly one return value:

```cpp
class ReadExtern : public kern::IExternFunction
{
public:
    kern::FunctionSignature getSignature() const override
    {
        return {0, kern::ReturnKind::Value};
    }
    bool call(kern::IExternCall &call) override
    {
        call.setReturnValue(42);
        return true;
    }
};
```

Void bindings do not set a return. A failed argument conversion or a `false`
return stops the script with `kern::RuntimeError`. The call context is valid
only during `call`.

## VM budgets

| Setting | Default | Scope |
| --- | --- | --- |
| `maxStackBytes` | 1 MiB | Runtime value slots, including locals and temporaries |
| `maxCallDepth` | 1024 | Nested script frames |
| `maxStringBytes` | 1 MiB | Each string or name decoded from bytecode |
| `maxTableEntries` | 1,048,576 | Each table and each function's instruction count |
| `maxExecutionSteps` | 10,000,000 | Instructions in one `callFunction` |
| `maxProgramBytes` | 64 MiB | Combined program tables, instructions, names, and constants |
| `maxRuntimeStringBytes` | 16 MiB | Live host-returned string contents |

The VM checks execution budget before each instruction. Budgets cannot
interrupt C++ code inside a binding. Program accounting bounds decoded data
before allocation. It counts stored objects and contents, including cached constant storage;
allocator overhead and spare capacity are excluded.
Strings are immutable and length-aware, including embedded zero bytes;
copies share storage. Host allocations and argument copies read into C++ are outside VM
budgets.

## Errors and recovery

Compilation and assembly return `false` and log diagnostics on invalid input.
Semantic checks include symbol resolution, full function signatures, stack
heights at control-flow joins, return values, and reachable fallthrough.
Value types and reads of uninitialized locals remain runtime checks.

Loading malformed or truncated bytecode returns `false`; budget violations
throw `kern::RuntimeError`. Either failure clears the loaded program while
leaving host bindings registered. `callFunction` returns `false` for an
unknown function. Script failures throw `kern::RuntimeError`; normal `ret`
and `exit` terminate successfully. Exceptions thrown by host code propagate.

Each call starts with clean value and call stacks, including after failure.
Calls run to completion. There is no yield or suspend API; split long work
into separate host calls as needed.

## Imports and diagnostics

The compile overload accepting search paths resolves `import name;` to
`name.ks`, using exact case-sensitive matching and the first matching path.
Missing files, cycles, mismatched module names, duplicate modules, and
conflicting extern signatures fail compilation. Semantic diagnostics from an
imported file retain its file name and source location.
