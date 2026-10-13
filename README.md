# KernScript

A bytecode interpreted scripting language for C++, with a small C-like
scripting language on top of an assembly dialect. General purpose embeddable
runtime: the host registers extern functions, loads scripts, and calls
script functions; scripts pull input through externs and push results back.

Scripts travel through four stages: the compiler lowers script source to
assembly text, the assembler encodes it to bytecode, and the virtual machine
executes it with host extern calls in between.

The language is dynamically typed (signed/unsigned int, float, string;
zero is false), single-return functions with explicit `return`, `if`/`else`,
`while`, C-like operator precedence, and string equality only. Each VM call
runs to completion; a bounded instruction count stops runaway scripts
instead of hanging the host. There are no modules yet (`import` is planned).

## Layout

- Library source: public API, product library, engine internals.
- Tests: unit, integration and end-to-end suites.
- Sample host applications, from minimal to script-driven UI.
- Release benchmarks with checksums and a recorded baseline.
- Language references: assembly instruction set, scripting grammar.

## Building

Requires CMake 4.4 or newer, Conan 2, and a C++23 compiler.

```sh
conan install . -s build_type=Debug --build=missing
cmake --preset conan-default
cmake --build build --config Debug
ctest --test-dir build -C Debug
```

The build type given to Conan must match the configuration given to CMake.
Substitute `Release` for `Debug` throughout to build the release
configuration (benchmarks are only meaningful in Release).

## License

MIT, see `LICENSE`.
