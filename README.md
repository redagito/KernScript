# KernScript

A bytecode interpreted scripting language for C++, with a small C-like source
language and a typed assembly interface. The host registers extern functions,
loads verified programs, and calls script functions; scripts pull input
through externs and push results back.

Scripts can compile directly to portable bytecode with `compileBytecode`, or
compile to typed assembly and pass through the assembler. The virtual machine
verifies loaded programs before execution.

The language is dynamically typed (signed/unsigned int, float, string;
numeric zero is false and all strings, including empty strings, are true),
functions with explicit `-> value` or `-> void` signatures,
`return`, `if`/`else`,
`while`, C-like operator precedence, and string equality only. Modules with
`import` and `export` organize multi-file programs. Each VM call
runs to completion; a bounded instruction count stops runaway scripts
instead of hanging the host.

## Layout

- Library source: public API, product library, engine internals.
- Tests: unit, integration and end-to-end suites.
- Sample host applications, from minimal to script-driven UI.
- Release benchmarks with checksums and a recorded baseline.
- Language references: assembly instruction set, scripting grammar.

## Building

Requires CMake 4.4.3 or newer, Conan 2, and a C++23 compiler.

```sh
conan install . -s build_type=Debug --build=missing
cmake --preset conan-default
cmake --build build --config Debug
ctest --test-dir build -C Debug
```

Tests are enabled by default. Samples and benchmarks are optional:

```sh
conan install . -s build_type=Release --build=missing -o "&:with_samples=True" -o "&:with_benchmarks=True"
```

Add `-o "&:with_gui_samples=True"` for the FLTK samples; only that option adds
FLTK as a dependency. Use `-o "&:with_tests=False"` for a library-only build
without Catch2. Conan forwards these options to CMake. When configuring
CMake directly, use `BUILD_TESTING`, `KERNSCRIPT_BUILD_SAMPLES`,
`KERNSCRIPT_BUILD_GUI_SAMPLES`, and `KERNSCRIPT_BUILD_BENCHMARKS`.

The build type given to Conan must match the configuration given to CMake.
Substitute `Release` for `Debug` throughout to build the release
configuration (benchmarks are only meaningful in Release).

## Documentation

- `docs/Getting Started.md`: build, first script, first extern.
- `docs/Host Manual.md`: API, budgets, ownership, errors.
- `docs/Scripting Cookbook.md`: runnable host/script patterns.
- `docs/Diagnostics Guide.md`: error catalog with fixes.
- `docs/Scripting Language.md`: grammar and semantics.
- `docs/Assembly Language.md`: instruction set.
- `docs/Migration 1.0.md`: breaking changes and upgrade steps.
- `Samples/README.md`: application catalog and running the samples.
- `docs/Sample Roadmap.md`: staged sample improvements and acceptance criteria.

## License

MIT, see `LICENSE`.
