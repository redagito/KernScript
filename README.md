# KernScript

KernScript is a bytecode interpreted scripting language for C++, built as an
educational project. It is not fit for productive use.

## Overview

A script travels through four stages, each implemented in `Lib/`:

| Stage | Component | Status |
| --- | --- | --- |
| Script source | `CCompiler` / `CParser` | Stub, `compile()` returns `false` |
| Assembly source | `CAssembler` | Works |
| Byte code file | `CAssembler` | Works |
| Execution | `CVirtualMachine` | Works |

The compiler does not exist yet. Scripts are currently written by hand in the
assembly dialect under `Test/EndToEndTest/data/asm/`, which is the only way to
reach the virtual machine today.

## Goals

* A high level scripting language that compiles down to assembly-like
  intermediate code.
* An assembler and a custom byte code file format.
* A virtual machine that executes byte code and binds to host functions.

## Current status

Roughly two thirds of the instruction set is implemented and covered by tests.
The gaps, all visible in `Instructions.h`, are the multiplication, division,
increment, decrement, logical and unconditional-jump families, plus the float
and string return instructions.

`CValue` also only implements integer arithmetic and comparison, and asserts on
anything else. Diagnostics in the assembler and lexer still write to `std::cout`
rather than a real logging channel.

## Building

Requires CMake 4.4 or newer, Conan 2, and a C++17 compiler.

```sh
conan install . -s build_type=Debug --build=missing
cmake --preset conan-default
cmake --build build --config Debug
ctest --test-dir build -C Debug
```

The build type passed to Conan must match the configuration passed to CMake.
Conan generates the Catch2 CMake configuration for a single build type at a
time, and the targets it exports guard their include and link paths by
configuration, so a Debug build against a Release install fails to find headers.

Substitute `Release` for `Debug` in all four commands to build the release
configuration.

## Layout

`Lib/` is the library, owning its public headers in `include/` and its private
headers in `source/`. `Test/` is an umbrella over four independent projects:
`TestSupport/` is a helper library, and `UnitTest/`, `IntegrationTest/` and
`EndToEndTest/` are the three Catch2 levels, each registering its own tests.

## License

MIT, see `LICENSE`.
