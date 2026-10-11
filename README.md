# KernScript

KernScript is a bytecode interpreted scripting language for C++, built as an
educational project. It is not fit for productive use.

## Overview

Scripts travel through four stages, each implemented in `Lib/`:

* Compiler — produces assembly-like code from script source.
* Assembler — produces byte code files in a custom format.
* Virtual machine — executes byte code files and calls into host functions.

The compiler is still a stub, so working scripts are currently written by hand
in the assembly dialect kept in `Test/EndToEndTest/data/asm/`.

## Goals

* A high level scripting language that compiles to assembly-like intermediate
  code.
* An assembler with a custom byte code file format.
* A virtual machine that runs byte code and binds to host functions.
* A test suite covering each stage in isolation and as a whole.

## Current status

The assembler and virtual machine work, and the test suite is green. The
compiler is unwritten, and parts of the instruction set are still missing. The
remaining gaps are listed in `Instructions.h`.

## Building

Requires CMake 4.4 or newer, Conan 2, and a C++17 compiler.

```sh
conan install . -s build_type=Debug --build=missing
cmake --preset conan-default
cmake --build build --config Debug
ctest --test-dir build -C Debug
```

The build type given to Conan must match the configuration given to CMake.
Conan writes the Catch2 configuration for one build type at a time, so a Debug
build against a Release install fails to find the headers.

`--config` is required because the Visual Studio generator used here is
multi-configuration: the configuration is chosen when building, not when
configuring. On a single-configuration generator such as Ninja, the
configuration is set with `-DCMAKE_BUILD_TYPE=Debug` at configure time and the
flag is dropped.

Substitute `Release` for `Debug` throughout to build the release configuration.

## Layout

`Lib/` is the library, owning its public headers in `include/` and its private
headers in `source/`. `Test/` is an umbrella over four independent projects:
`TestSupport/` is a helper library, and `UnitTest/`, `IntegrationTest/` and
`EndToEndTest/` are the three Catch2 levels, each registering its own tests.

## License

MIT, see `LICENSE`.
