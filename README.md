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

- `Lib/` — `ApiHeaders` (public `kern/*` API), `Api` (product library
  `KernScriptLib`, factories only), `Impl` (`include/` + `source/`, the
  engine: lexer, assembler, compiler, VM).
- `Test/` — Catch2 unit, integration and end-to-end suites.
- `Samples/` — host applications linking `KernScriptLib`: minimal hosts,
  calculator, text pipeline, rule engine, FLTK/Win32 UI demos, script-driven
  Life, task board, spreadsheet, Eliza, and a script-built UI shell.
- `Benchmarks/` — per-stage Release benchmarks with checksums plus a
  recorded baseline (`Benchmarks/README.md`); `NestedCalls` and
  `ExternRoundtrip` take `--profile` for opcode tables.
- `doc/` — `Assembly Language.md` (instruction reference),
  `Scripting Language.md` (grammar and semantics).

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
